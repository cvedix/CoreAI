
#include "cvedix_node.h"

namespace cvedix_nodes {
    
    cvedix_node::cvedix_node(std::string node_name): node_name(node_name) {
    }
    
    cvedix_node::~cvedix_node() {

    }

    // Thread-safe access for in_queue and out_queue must be enforced.
    void cvedix_node::handle_run() {
        // cache for batch handling if need
        std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>> frame_meta_batch_cache;
        while (alive) {
            // wait for producer, make sure in_queue is not empty.
            this->in_queue_semaphore.wait();

            std::shared_ptr<cvedix_objects::cvedix_meta> in_meta;
            {
                std::lock_guard<std::mutex> guard(this->in_queue_lock);
                CVEDIX_DEBUG(cvedix_utils::string_format("[%s] before handling meta, in_queue.size()==>%d", node_name.c_str(), in_queue.size()));
                in_meta = this->in_queue.front();
            }
            
            // dead flag
            if (in_meta == nullptr) {
                continue;
            }
            
            // handling hooker activated if need
            invoke_meta_handling_hooker(node_name, in_queue.size(), in_meta);

            std::shared_ptr<cvedix_objects::cvedix_meta> out_meta;
            auto batch_complete = false;

            // call handlers
            if (in_meta->meta_type == cvedix_objects::cvedix_meta_type::CONTROL) {
                auto meta_2_handle = std::dynamic_pointer_cast<cvedix_objects::cvedix_control_meta>(in_meta);
                out_meta = this->handle_control_meta(meta_2_handle);
            }
            else if (in_meta->meta_type == cvedix_objects::cvedix_meta_type::FRAME) {    
                auto meta_2_handle = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(in_meta);
                // one by one
                if (frame_meta_handle_batch == 1) {                    
                    out_meta = this->handle_frame_meta(meta_2_handle);
                } 
                else {
                    // batch by batch
                    frame_meta_batch_cache.push_back(meta_2_handle);
                    if (frame_meta_batch_cache.size() >= frame_meta_handle_batch) {
                        // cache complete
                        this->handle_frame_meta(frame_meta_batch_cache);
                        batch_complete = true;
                    } 
                    else {
                        // cache not complete, do nothing
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] handle meta with batch, frame_meta_batch_cache.size()==>%d", node_name.c_str(), frame_meta_batch_cache.size()));
                    }
                }
            }
            else {
                throw "invalid meta type!";
            }
            {
                std::lock_guard<std::mutex> guard(this->in_queue_lock);
                this->in_queue.pop();
                CVEDIX_DEBUG(cvedix_utils::string_format("[%s] after handling meta, in_queue.size()==>%d", node_name.c_str(), in_queue.size()));
            }

            // one by one mode
            // return nullptr means do not push it to next nodes(such as in des nodes).
            if (out_meta != nullptr && node_type() != cvedix_node_type::DES) {
                {
                    std::lock_guard<std::mutex> guard(this->out_queue_lock);
                    CVEDIX_DEBUG(cvedix_utils::string_format("[%s] before handling meta, out_queue.size()==>%d", node_name.c_str(), out_queue.size()));
                    this->out_queue.push(out_meta);
                }

                // handled hooker activated if need
                invoke_meta_handled_hooker(node_name, out_queue.size(), out_meta);

                // notify consumer of out_queue
                this->out_queue_semaphore.signal();
                {
                    std::lock_guard<std::mutex> guard(this->out_queue_lock);
                    CVEDIX_DEBUG(cvedix_utils::string_format("[%s] after handling meta, out_queue.size()==>%d", node_name.c_str(), out_queue.size()));
                }
            }

            // batch by batch mode
            if (batch_complete && node_type() != cvedix_node_type::DES) {
                // push to out_queue one by one
                for (auto& i: frame_meta_batch_cache) {
                    {
                        std::lock_guard<std::mutex> guard(this->out_queue_lock);
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] before handling meta, out_queue.size()==>%d", node_name.c_str(), out_queue.size()));
                        this->out_queue.push(i);
                    }

                    // handled hooker activated if need
                    invoke_meta_handled_hooker(node_name, out_queue.size(), i);

                    // notify consumer of out_queue
                    this->out_queue_semaphore.signal();
                    {
                        std::lock_guard<std::mutex> guard(this->out_queue_lock);
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] after handling meta, out_queue.size()==>%d", node_name.c_str(), out_queue.size()));
                    }
                }
                // clean cache for the next batch
                frame_meta_batch_cache.clear();
            }
        }
        // send dead flag for dispatch_thread
        {
            std::lock_guard<std::mutex> guard(this->out_queue_lock);
            this->out_queue.push(nullptr);
        }
        this->out_queue_semaphore.signal();
    }

    void cvedix_node::dispatch_run() {
        while (alive) {
            // wait for producer, make sure out_queue is not empty.
            this->out_queue_semaphore.wait();

            std::shared_ptr<cvedix_objects::cvedix_meta> out_meta;
            size_t current_out_size = 0;
            {
                std::lock_guard<std::mutex> guard(this->out_queue_lock);
                current_out_size = this->out_queue.size();
                CVEDIX_DEBUG(cvedix_utils::string_format("[%s] before dispatching meta, out_queue.size()==>%d", node_name.c_str(), current_out_size));
                out_meta = this->out_queue.front();
                this->out_queue.pop();
            }
            // dead flag
            if (out_meta == nullptr) {
                continue;
            }

            // leaving hooker activated if need
            invoke_meta_leaving_hooker(node_name, current_out_size, out_meta);

            // do something..
            this->push_meta(out_meta);
            {
                std::lock_guard<std::mutex> guard(this->out_queue_lock);
                CVEDIX_DEBUG(cvedix_utils::string_format("[%s] after dispatching meta, out_queue.size()==>%d", node_name.c_str(), this->out_queue.size()));
            }
        }
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
        return meta;
    }

    void cvedix_node::handle_frame_meta(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& meta_with_batch) {
        
    }

    void cvedix_node::meta_flow(std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
        if (meta == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> guard(this->in_queue_lock);
        // drop meta if queue is full, but throttle warning logs to avoid self-inflicted CPU spikes
        if (this->in_queue.size() >= max_in_queue_size) {
            dropped_meta_since_warn++;
            const auto now = std::chrono::steady_clock::now();
            const auto since_last_warn = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_drop_warn_time).count();
            if (last_drop_warn_time.time_since_epoch().count() == 0 || since_last_warn >= 1000) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] queue full, dropped %d meta in the last %lld ms (queue=%zu, max=%d)",
                    node_name.c_str(),
                    dropped_meta_since_warn,
                    static_cast<long long>(since_last_warn < 0 ? 0 : since_last_warn),
                    this->in_queue.size(),
                    max_in_queue_size));
                dropped_meta_since_warn = 0;
                last_drop_warn_time = now;
            }
            return;
        }
        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] before meta flow, in_queue.size()==>%d", node_name.c_str(), in_queue.size()));
        this->in_queue.push(meta);

        // arriving hooker activated if need
        invoke_meta_arriving_hooker(node_name, in_queue.size(), meta);
        
        // notify consumer of in_queue
        this->in_queue_semaphore.signal();
        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] after meta flow, in_queue.size()==>%d", node_name.c_str(), in_queue.size()));
    }

    void cvedix_node::detach() {
        for(auto i : this->pre_nodes) {
            i->remove_subscriber(shared_from_this());
        }
        this->pre_nodes.clear();
    }

    void cvedix_node::detach_from(std::vector<std::string> pre_node_names) {
        for (auto i = this->pre_nodes.begin(); i != this->pre_nodes.end();) {
            if (std::find(pre_node_names.begin(), pre_node_names.end(), (*i)->node_name) != pre_node_names.end()) {
                (*i)->remove_subscriber(shared_from_this());
                i = this->pre_nodes.erase(i);
            }
            else {
                i++;
            }
        }
    }

    void cvedix_node::detach_recursively() {
        detach();
        auto nodes = next_nodes();
        for (auto& n: nodes) {
            n->detach_recursively();
        }
    }

    void cvedix_node::attach_to(std::vector<std::shared_ptr<cvedix_node>> pre_nodes) {
        // can not attach src node to any previous nodes
        if (this->node_type() == cvedix_node_type::SRC) {
            throw cvedix_excepts::cvedix_invalid_calling_error("SRC nodes must not have any previous nodes!");
        }
        // can not attach any nodes to des node
        for(auto i : pre_nodes) {
            if (i->node_type() == cvedix_node_type::DES) {
                throw cvedix_excepts::cvedix_invalid_calling_error("DES nodes must not have any next nodes!");
            }
            i->add_subscriber(shared_from_this());
            this->pre_nodes.push_back(i);
        }
    }

    void cvedix_node::initialized() {

        if (handle_thread.joinable() || dispatch_thread.joinable()) {
            throw std::runtime_error("Node already initialized");
        }

        handle_thread = std::thread(&cvedix_node::handle_run, this);
        dispatch_thread = std::thread(&cvedix_node::dispatch_run, this);
    }

    void cvedix_node::deinitialized() {
        // send dead flag
        alive = false;
        {
            std::lock_guard<std::mutex> guard(this->in_queue_lock);
            this->in_queue.push(nullptr);
            this->in_queue_semaphore.signal();
        }
        // wait for threads exits in cvedix_node
        if (handle_thread.joinable()) {
            handle_thread.join();
        }
        if (dispatch_thread.joinable()) {
            dispatch_thread.join();
        }
    }

    cvedix_node_type cvedix_node::node_type() {
        // return cvedix_node_type::MID by default
        // need override in child class
        return cvedix_node_type::MID;
    }

    std::vector<std::shared_ptr<cvedix_node>> cvedix_node::next_nodes() {
        std::vector<std::shared_ptr<cvedix_node>> next_nodes;
        std::lock_guard<std::mutex> guard(this->subscribers_lock);
        for(auto & i: this->subscribers) {
            next_nodes.push_back(std::dynamic_pointer_cast<cvedix_node>(i));
        }
        return next_nodes;
    }

    std::string cvedix_node::to_string() {
        // return node_name by default
        return node_name;
    }

    void cvedix_node::pendding_meta(std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
        {
            std::lock_guard<std::mutex> guard(this->out_queue_lock);
            this->out_queue.push(meta);
        }
        // handled hooker activated if need
        invoke_meta_handled_hooker(node_name, out_queue.size(), meta);        
        // notify consumer of out_queue
        this->out_queue_semaphore.signal();
    }
}