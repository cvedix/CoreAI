

#include <string>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include "../common/cvedix_src_node.h"

namespace cvedix_nodes {
    // udp source node, receive video stream via udp(rtp) protocal.
    // example:
    // udp://127.0.0.1:6000
    class cvedix_udp_src_node: public cvedix_src_node
    {
    private:
        std::string gst_template = "udpsrc port=%d ! application/x-rtp,mdeia=video ! rtph264depay ! h264parse ! %s ! videoconvert ! appsink";
        cv::VideoCapture udp_capture;
    protected:
        // re-implemetation
        virtual void handle_run() override;
    public:
        cvedix_udp_src_node(std::string node_name, 
                        int channel_index, 
                        int port, 
                        float resize_ratio = 1.0,
                        std::string gst_decoder_name = "avdec_h264",
                        int skip_interval = 0);
        ~cvedix_udp_src_node();

        virtual std::string to_string() override;

        // port to listen
        int port;
        // set avdec_h264 as the default decoder, we can use hardware decoder instead.
        std::string gst_decoder_name = "avdec_h264";
        // 0 means no skip
        int skip_interval = 0;
    };
}