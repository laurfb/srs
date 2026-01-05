/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 */

#include <srs_protocol_rist.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_protocol_utility.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

SrsRistSocket::SrsRistSocket()
{
    ctx_ = NULL;
    peer_ = NULL;
    stfd_ = NULL;
    fd_ = -1;
    is_sender_ = false;
    is_listener_ = false;
}

SrsRistSocket::~SrsRistSocket()
{
    stop();
}

srs_error_t SrsRistSocket::initialize(bool is_sender, bool is_listener)
{
    srs_error_t err = srs_success;
    
    is_sender_ = is_sender;
    is_listener_ = is_listener;
    
    // Create RIST context
    int profile = is_sender ? RIST_PROFILE_SIMPLE : RIST_PROFILE_MAIN;
    
    if (rist_sender_create(&ctx_, profile, 0, NULL) != 0) {
        return srs_error_new(ERROR_RIST_INIT, "create RIST context failed");
    }
    
    // Setup logging
    struct rist_logging_settings log_settings;
    memset(&log_settings, 0, sizeof(log_settings));
    log_settings.log_level = RIST_LOG_INFO;
    log_settings.log_cb = NULL;  // Use default logging
    log_settings.log_cb_arg = NULL;
    
    if (rist_logging_set(&log_settings, ctx_) != 0) {
        srs_warn("RIST: failed to setup logging");
    }
    
    return err;
}

srs_error_t SrsRistSocket::add_peer(std::string url)
{
    srs_error_t err = srs_success;
    
    if (!ctx_) {
        return srs_error_new(ERROR_RIST_INIT, "RIST context not initialized");
    }
    
    // Parse URL
    const struct rist_peer_config* peer_config = NULL;
    
    if (rist_parse_address2(url.c_str(), &peer_config) != 0) {
        return srs_error_new(ERROR_RIST_PARSE, "parse RIST URL failed: %s", url.c_str());
    }
    
    // Add peer
    if (is_sender_) {
        if (rist_sender_peer_create(ctx_, &peer_, peer_config) != 0) {
            return srs_error_new(ERROR_RIST_PEER, "create RIST sender peer failed");
        }
    } else {
        if (rist_receiver_peer_create(ctx_, &peer_, peer_config) != 0) {
            return srs_error_new(ERROR_RIST_PEER, "create RIST receiver peer failed");
        }
    }
    
    // Free config
    rist_peer_config_free2(&peer_config);
    
    return err;
}

srs_error_t SrsRistSocket::start()
{
    srs_error_t err = srs_success;
    
    if (!ctx_) {
        return srs_error_new(ERROR_RIST_INIT, "RIST context not initialized");
    }
    
    // Start RIST
    if (is_sender_) {
        if (rist_start(ctx_) != 0) {
            return srs_error_new(ERROR_RIST_START, "start RIST sender failed");
        }
    } else {
        if (rist_start(ctx_) != 0) {
            return srs_error_new(ERROR_RIST_START, "start RIST receiver failed");
        }
    }
    
    // Get fd for ST integration
    int fds[16];
    int num_fds = rist_get_fd(ctx_, fds, 16);
    if (num_fds > 0) {
        fd_ = fds[0];  // Use first fd
        if ((err = convert_to_stfd()) != srs_success) {
            return srs_error_wrap(err, "convert to stfd");
        }
    }
    
    return err;
}

void SrsRistSocket::stop()
{
    if (ctx_) {
        rist_destroy(ctx_);
        ctx_ = NULL;
    }
    
    peer_ = NULL;
    
    if (stfd_) {
        srs_close_stfd(stfd_);
        stfd_ = NULL;
    }
    
    fd_ = -1;
}

int SrsRistSocket::get_fd()
{
    return fd_;
}

void SrsRistSocket::set_fd(int fd)
{
    fd_ = fd;
}

srs_error_t SrsRistSocket::recvmsg(void* buf, size_t* size, srs_utime_t timeout)
{
    srs_error_t err = srs_success;
    
    if (!ctx_) {
        return srs_error_new(ERROR_RIST_RECV, "RIST context not initialized");
    }
    
    // Convert timeout to milliseconds
    int timeout_ms = (int)(timeout / 1000);
    
    while (true) {
        // Try to receive data
        const struct rist_data_block* block = NULL;
        int ret = rist_receiver_data_read2(ctx_, &block, timeout_ms);
        
        if (ret > 0 && block) {
            // Copy data to buffer
            size_t copy_size = srs_min(*size, (size_t)block->payload_len);
            memcpy(buf, block->payload, copy_size);
            *size = copy_size;
            
            // Free block
            rist_receiver_data_block_free2(&block);
            
            return err;
        } else if (ret == 0) {
            // Timeout or would block
            if (stfd_) {
                // Wait for data using ST
                if (st_netfd_poll(stfd_, POLLIN, timeout) <= 0) {
                    return srs_error_new(ERROR_SOCKET_TIMEOUT, "RIST recv timeout");
                }
                // Try again
                continue;
            } else {
                return srs_error_new(ERROR_SOCKET_TIMEOUT, "RIST recv timeout");
            }
        } else {
            // Error
            return srs_error_new(ERROR_RIST_RECV, "RIST recv error: %d", ret);
        }
    }
    
    return err;
}

srs_error_t SrsRistSocket::sendmsg(const void* buf, size_t size, srs_utime_t timeout)
{
    srs_error_t err = srs_success;
    
    if (!ctx_) {
        return srs_error_new(ERROR_RIST_SEND, "RIST context not initialized");
    }
    
    // Create data block
    struct rist_data_block block;
    memset(&block, 0, sizeof(block));
    block.payload = (void*)buf;
    block.payload_len = size;
    block.ts_ntp = 0;  // Let library assign timestamp
    
    // Send data
    int ret = rist_sender_data_write(ctx_, &block);
    if (ret < 0) {
        return srs_error_new(ERROR_RIST_SEND, "RIST send error: %d", ret);
    }
    
    return err;
}

srs_error_t SrsRistSocket::get_stats(struct rist_stats* stats)
{
    srs_error_t err = srs_success;
    
    if (!ctx_) {
        return srs_error_new(ERROR_RIST_STATS, "RIST context not initialized");
    }
    
    const struct rist_stats_sender_peer* sender_stats = NULL;
    const struct rist_stats_receiver_flow* receiver_stats = NULL;
    
    if (is_sender_) {
        if (rist_stats_sender_peer(ctx_, &sender_stats) != 0) {
            return srs_error_new(ERROR_RIST_STATS, "get RIST sender stats failed");
        }
        // Copy relevant stats
        rist_stats_free(sender_stats);
    } else {
        if (rist_stats_receiver_flow(ctx_, &receiver_stats) != 0) {
            return srs_error_new(ERROR_RIST_STATS, "get RIST receiver stats failed");
        }
        // Copy relevant stats
        rist_stats_free(receiver_stats);
    }
    
    return err;
}

bool SrsRistSocket::is_connected()
{
    return ctx_ != NULL && peer_ != NULL;
}

srs_error_t SrsRistSocket::set_option(const char* key, const char* value)
{
    srs_error_t err = srs_success;
    
    // Set RIST-specific options here
    // This can be extended based on librist capabilities
    
    return err;
}

srs_error_t SrsRistSocket::convert_to_stfd()
{
    srs_error_t err = srs_success;
    
    if (fd_ < 0) {
        return srs_error_new(ERROR_SOCKET_CREATE, "invalid fd");
    }
    
    stfd_ = st_netfd_open_socket(fd_);
    if (!stfd_) {
        return srs_error_new(ERROR_SOCKET_CREATE, "convert fd to stfd failed");
    }
    
    return err;
}

int SrsRistSocket::auth_handler(void* arg, const char* connecting_ip, uint16_t connecting_port,
                               const char* local_ip, uint16_t local_port, struct rist_peer* peer)
{
    // Authentication logic
    srs_trace("RIST: auth request from %s:%d", connecting_ip, connecting_port);
    return 0;  // Accept
}

int SrsRistSocket::connect_handler(void* arg, const char* ip, uint16_t port,
                                  const char* local_ip, uint16_t local_port, struct rist_peer* peer)
{
    srs_trace("RIST: connected to %s:%d", ip, port);
    return 0;
}

int SrsRistSocket::disconnect_handler(void* arg, struct rist_peer* peer)
{
    srs_trace("RIST: disconnected");
    return 0;
}

// SrsRistUrl implementation

SrsRistUrl::SrsRistUrl()
{
    port_ = 0;
}

SrsRistUrl::~SrsRistUrl()
{
}

srs_error_t SrsRistUrl::parse(const std::string& url)
{
    srs_error_t err = srs_success;
    
    url_ = url;
    
    // Simple RIST URL parser: rist://host:port?params
    size_t pos = url.find("://");
    if (pos == std::string::npos) {
        return srs_error_new(ERROR_RIST_PARSE, "invalid RIST URL: %s", url.c_str());
    }
    
    std::string rest = url.substr(pos + 3);
    
    // Extract host:port
    size_t param_pos = rest.find('?');
    std::string host_port = (param_pos != std::string::npos) ? rest.substr(0, param_pos) : rest;
    
    size_t colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos) {
        host_ = host_port.substr(0, colon_pos);
        port_ = atoi(host_port.substr(colon_pos + 1).c_str());
    } else {
        host_ = host_port;
        port_ = 1971;  // Default RIST port
    }
    
    // Parse parameters
    if (param_pos != std::string::npos) {
        std::string params = rest.substr(param_pos + 1);
        // Simple parameter parsing (key=value&key=value)
        size_t start = 0;
        while (start < params.length()) {
            size_t amp = params.find('&', start);
            std::string param = (amp != std::string::npos) ? 
                params.substr(start, amp - start) : params.substr(start);
            
            size_t eq = param.find('=');
            if (eq != std::string::npos) {
                std::string key = param.substr(0, eq);
                std::string value = param.substr(eq + 1);
                params_[key] = value;
                
                if (key == "mode") {
                    mode_ = value;
                }
            }
            
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }
    
    return err;
}

std::string SrsRistUrl::get_param(const std::string& key) const
{
    auto it = params_.find(key);
    return (it != params_.end()) ? it->second : "";
}

std::string SrsRistUrl::to_string() const
{
    return url_;
}
