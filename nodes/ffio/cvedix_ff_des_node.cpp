#include "cvedix_ff_des_node.h"

namespace cvedix_nodes {
    cvedix_ff_des_node::
    cvedix_ff_des_node(const std::string& node_name,
                          int channel_index,
                          const std::string& out_uri,
                          cvedix_objects::cvedix_size resolution_w_h, 
                          int bitrate,
                          bool osd,
                          std::string encoder_name):
                          cvedix_des_node(node_name, channel_index),
                          m_out_uri(out_uri),
                          m_resolution_w_h(resolution_w_h),
                          m_out_bitrate(bitrate),
                          m_use_osd(osd),
                          m_encoder_name(encoder_name) {
        m_ff_des = alloc_ff_des(channel_index);
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [%s]", node_name.c_str(), out_uri.c_str()));
        this->initialized();
    }

    cvedix_ff_des_node::~cvedix_ff_des_node() {
        deinitialized();
        if (sws_ctx) {
            sws_freeContext(sws_ctx);
        }
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_ff_des_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] received frame meta, channel_index=>%d, frame_index=>%d", node_name.c_str(), meta->channel_index, meta->frame_index));
        
        cv::Mat resize_frame = (m_use_osd && !meta->osd_frame.empty()) ? meta->osd_frame : meta->frame;
        auto out_width = resize_frame.cols;
        auto out_height = resize_frame.rows;
        if (m_resolution_w_h.width != 0 && m_resolution_w_h.height != 0) {
            out_width = m_resolution_w_h.width;
            out_height = m_resolution_w_h.height;
        }
              
        /* try to open ff_dec. */
        if (!m_ff_des->is_opened()) {
            if(!m_ff_des->open(m_out_uri, 
                                out_width, 
                                out_height, 
                                meta->fps, 
                                m_out_bitrate, 
                                0, 
                                m_encoder_name, 
                                AV_PIX_FMT_YUV420P)) {
                CVEDIX_WARN(cvedix_utils::string_format("[%s] could not open ff_des.", node_name.c_str()));
                /* general works in cvedix_des_node. */
                return cvedix_des_node::handle_frame_meta(meta);
            }
        }

        /* initialize sws_ctx. */
        if (!sws_ctx) {
            sws_ctx = sws_getContext(resize_frame.cols, 
                                     resize_frame.rows, 
                                     AV_PIX_FMT_BGR24, 
                                     out_width, 
                                     out_height, 
                                     AV_PIX_FMT_YUV420P, 
                                     0, NULL, NULL, NULL);
        }
        if (!sws_ctx) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] could not initialize sws_ctx.", node_name.c_str()));
            /* general works in cvedix_des_node. */
            return cvedix_des_node::handle_frame_meta(meta);
        }
        
        /* cv::Mat -> AVFrame. */
        /* resize and convert to AV_PIX_FMT_YUV420P. */
        auto dst_frame = alloc_ff_av_frame();
        dst_frame->width = out_width;
        dst_frame->height = out_height;
        dst_frame->format= AV_PIX_FMT_YUV420P;
        av_frame_get_buffer(dst_frame.get(), 0);
        
        auto p = resize_frame.data;
        int linesize[1] = {resize_frame.cols * 3};
        sws_scale(sws_ctx, &p, linesize, 0, resize_frame.rows, dst_frame->data, dst_frame->linesize);

        /* write to ff_des. */
        m_ff_des->write(dst_frame);

        /* general works in cvedix_des_node. */
        return cvedix_des_node::handle_frame_meta(meta);
    }

    std::string cvedix_ff_des_node::to_string() {
        return m_out_uri;
    }
}