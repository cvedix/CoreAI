/**
 * @file test_rtmp_des_node.cpp
 * @brief Tests for cvedix_rtmp_des_node's failure detection.
 *
 * The node exists to push video to an RTMP server, and the thing that used to
 * be broken about it was not the video: it was the reporting. Built on
 * cv::VideoWriter with CAP_GSTREAMER, it connected asynchronously and learned
 * about failures only as log warnings on a bus it could not reach. Measured
 * against a port with nothing listening, the old implementation called
 * `open()` successfully, threw nothing, and lost 99 of 100 frames in silence --
 * so its reconnect block never ran once.
 *
 * That is what `rtmp_des_detects_unreachable_server` pins down. It needs no
 * server, so it runs everywhere including CI, and it fails against the old
 * implementation.
 *
 * `rtmp_des_pushes_to_live_server` is the positive counterpart and skips itself
 * when there is no server to talk to.
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"

#include "test_assert.h"
#include "test_nodes.h"

namespace {

/// @brief Small enough that encoding 50 frames costs nothing on CPU
constexpr int kWidth = 160;
constexpr int kHeight = 120;
constexpr int kBitrate = 256;
/// @brief x264enc is CPU-only, so the tests do not require NVENC
constexpr const char* kEncoder = "x264enc";

/**
 * @brief Ask the kernel for a TCP port that is free, then let it go.
 *
 * Connecting there is then as close to "nothing is listening" as a test can
 * get. Nothing keeps the port reserved after the close, so in principle another
 * process could take it -- the window is microseconds and the worst case is a
 * flaky test rather than a wrong one.
 *
 * @return the port, or -1 when it could not be determined
 */
int reserve_then_release_free_port() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return -1;
    }
    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        ::close(fd);
        return -1;
    }
    const int port = static_cast<int>(::ntohs(addr.sin_port));
    ::close(fd);
    return port;
}

/// @brief Whether something accepts a TCP connection at @p host : @p port .
bool tcp_port_open(const std::string& host, int port, int timeout_ms) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(static_cast<uint16_t>(port));
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        ::close(fd);
        return false;
    }

    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    bool open = false;
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
        open = true;
    } else if (errno == EINPROGRESS) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(fd, &writable);
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (::select(fd + 1, nullptr, &writable, nullptr, &tv) == 1) {
            int socket_error = 0;
            socklen_t len = sizeof(socket_error);
            if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &len) == 0 &&
                socket_error == 0) {
                open = true;
            }
        }
    }
    ::close(fd);
    return open;
}

/// @brief Build the node every case uses, with the URL taken verbatim.
std::shared_ptr<cvedix_nodes::cvedix_rtmp_des_node> make_rtmp_node(
        const std::string& node_name, const std::string& url) {
    // append_channel_suffix is false so the URL reaching the sink is exactly the
    // one given here -- otherwise the node would silently publish to "<url>_0".
    return std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
        node_name, 0, url, cvedix_objects::cvedix_size{kWidth, kHeight},
        kBitrate, false /*osd*/, kEncoder, false /*append_channel_suffix*/);
}

}  // namespace

// --------------------------------------------------------------------------
// Failure detection -- the regression this node was rewritten for
// --------------------------------------------------------------------------

CVEDIX_TEST_CASE(rtmp_des_detects_unreachable_server) {
    const int dead_port = reserve_then_release_free_port();
    if (dead_port <= 0) {
        CVEDIX_FAIL("could not reserve a local TCP port");
    }

    auto node = make_rtmp_node(
        "rtmp_unreachable",
        "rtmp://127.0.0.1:" + std::to_string(dead_port) + "/live/unreachable");

    // Frames are offered over time rather than in one burst. The failure is
    // reported on the GStreamer bus, and this node only reads the bus while it
    // is handling a frame -- so a burst followed by silence would never see it.
    int offered = 0;
    const bool detected = cvedix_test::wait_for(
        [&] {
            node->meta_flow(cvedix_test::make_frame(offered++, 0));
            return node->failure_count() > 0;
        },
        std::chrono::milliseconds(10000));

    CVEDIX_ASSERT_TRUE(detected);
    CVEDIX_ASSERT_FALSE(node->is_streaming());
    // The old implementation accepted every frame and counted no failures, so
    // this is the assertion that separates the two: some frames were offered,
    // and the node admits most of them never made it.
    CVEDIX_ASSERT_TRUE(node->frames_pushed() < offered);

    // Unlike the test doubles in test_nodes.h, the real node exposes no public
    // stop(): cvedix_node shuts down in its destructor, which this calls
    // through the shared_ptr.
    node.reset();
}

// --------------------------------------------------------------------------
// Pipeline shape -- the three properties that decide whether a viewer sees a
// clean picture or a torn one
// --------------------------------------------------------------------------

/// @brief Everything in @p pipeline from @p marker onwards, or "" if absent
std::string tail_after(const std::string& pipeline, const std::string& marker) {
    const size_t at = pipeline.find(marker);
    return at == std::string::npos ? std::string() : pipeline.substr(at);
}

CVEDIX_TEST_CASE(rtmp_des_pipeline_keeps_the_bitstream_intact) {
    auto node = make_rtmp_node("rtmp_shape", "rtmp://127.0.0.1:1935/live/shape");
    const std::string pipeline = node->pipeline_description();

    // 1. Nothing downstream of the muxer may drop a buffer. A dropped FLV tag
    //    breaks the H.264 reference chain, which is what made the published
    //    stream unreadable while the recorded MP4 stayed clean.
    const std::string after_mux = tail_after(pipeline, "flvmux");
    CVEDIX_ASSERT_FALSE(after_mux.empty());
    CVEDIX_ASSERT_TRUE(after_mux.find("leaky") == std::string::npos);

    // 2. Frames may only be dropped upstream of the encoder, where a lost frame
    //    costs one frame of video and nothing more.
    CVEDIX_ASSERT_TRUE(tail_after(pipeline, "appsrc").find("leaky=downstream") != std::string::npos);

    // 3. A keyframe has to arrive soon enough to bound the damage. x264enc's
    //    own default is 250 frames, which is tens of seconds at these rates.
    CVEDIX_ASSERT_TRUE(pipeline.find("key-int-max=") != std::string::npos);
    CVEDIX_ASSERT_TRUE(pipeline.find("key-int-max=250") == std::string::npos);

    // 4. The encoder must be handed 4:2:0. Left to negotiate, videoconvert and
    //    x264enc settle on Y444 and publish High 4:4:4 Predictive, which many
    //    players and repackagers refuse to decode.
    CVEDIX_ASSERT_TRUE(pipeline.find("format=I420") != std::string::npos);

    node.reset();
}

CVEDIX_TEST_CASE(rtmp_des_pipeline_sets_gop_for_nvenc) {
    // The NVENC branch is built as a string like any other, so the element does
    // not have to be installed to check it. NVENC's gop-size default is also
    // 250, and leaving it out is the same bug the x264enc branch had.
    auto node = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
        "rtmp_nvenc_shape", 0, "rtmp://127.0.0.1:1935/live/nvenc",
        cvedix_objects::cvedix_size{kWidth, kHeight}, kBitrate, false /*osd*/,
        "nvh264enc", false /*append_channel_suffix*/);

    const std::string pipeline = node->pipeline_description();
    CVEDIX_ASSERT_TRUE(pipeline.find("nvh264enc") != std::string::npos);
    CVEDIX_ASSERT_TRUE(pipeline.find("gop-size=") != std::string::npos);
    CVEDIX_ASSERT_TRUE(pipeline.find("gop-size=250") == std::string::npos);

    node.reset();
}

// --------------------------------------------------------------------------
// Happy path -- skips when no server is reachable
// --------------------------------------------------------------------------

CVEDIX_TEST_CASE(rtmp_des_pushes_to_live_server) {
    const char* host_env = std::getenv("CVEDIX_TEST_RTMP_HOST");
    const char* port_env = std::getenv("CVEDIX_TEST_RTMP_PORT");
    const std::string host = host_env != nullptr ? host_env : "127.0.0.1";
    const int port = port_env != nullptr ? std::atoi(port_env) : 1935;

    if (!tcp_port_open(host, port, 500)) {
        // Matches the convention in tests/README.md: an external dependency that
        // is absent skips rather than fails, so the suite stays runnable in CI.
        std::cout << "[  SKIP  ] no RTMP server listening at " << host << ":" << port
                  << " -- start one, or point CVEDIX_TEST_RTMP_HOST/"
                     "CVEDIX_TEST_RTMP_PORT at it" << std::endl;
        return;
    }

    auto node = make_rtmp_node(
        "rtmp_live",
        "rtmp://" + host + ":" + std::to_string(port) + "/live/cvedix_test");

    constexpr int kFrames = 50;
    for (int i = 0; i < kFrames; ++i) {
        node->meta_flow(cvedix_test::make_frame(i, 0));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    CVEDIX_ASSERT_TRUE(cvedix_test::wait_for([&] { return node->frames_pushed() > 0; }));
    CVEDIX_ASSERT_EQ(node->failure_count(), 0);
    CVEDIX_ASSERT_TRUE(node->is_streaming());

    // Worth being precise about what this proves: that the pipeline builds, runs
    // and reports no error. It does NOT prove the server accepted the stream --
    // RTMP has no acknowledgement to wait for, and confirming delivery needs the
    // server's own API (for ZLMediaKit, /index/api/getMediaList).
    node.reset();
}

int main(int argc, char** argv) {
    // Streaming nodes log per frame; at the default level that would bury the
    // test output.
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::ERROR);
    CVEDIX_SET_LOG_TO_FILE(false);
    CVEDIX_LOGGER_INIT();
    return cvedix_test::run_all(argc, argv);
}
