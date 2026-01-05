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

#ifndef SRS_PROTOCOL_RIST_HPP
#define SRS_PROTOCOL_RIST_HPP

#include <srs_core.hpp>
#include <srs_protocol_st.hpp>

#include <string>
#include <librist/librist.h>

class SrsRistSocket
{
private:
    struct rist_ctx* ctx_;
    struct rist_peer* peer_;
    stfd_t stfd_;
    int fd_;
    bool is_sender_;
    bool is_listener_;
    
public:
    SrsRistSocket();
    virtual ~SrsRistSocket();

public:
    // Initialize RIST context and peer
    srs_error_t initialize(bool is_sender, bool is_listener);
    
    // Add peer with URL
    srs_error_t add_peer(std::string url);
    
    // Start RIST session
    srs_error_t start();
    
    // Stop RIST session
    void stop();
    
    // Get the underlying fd for ST integration
    int get_fd();
    
    // Set fd for ST coroutine (after accept)
    void set_fd(int fd);
    
    // Receive data (coroutine-safe)
    srs_error_t recvmsg(void* buf, size_t* size, srs_utime_t timeout);
    
    // Send data (coroutine-safe)
    srs_error_t sendmsg(const void* buf, size_t size, srs_utime_t timeout);
    
    // Get statistics
    srs_error_t get_stats(struct rist_stats* stats);
    
    // Check if connected
    bool is_connected();
    
    // Set socket options
    srs_error_t set_option(const char* key, const char* value);

private:
    // Convert fd to st fd for coroutine integration
    srs_error_t convert_to_stfd();
    
    // RIST callback handlers
    static int auth_handler(void* arg, const char* connecting_ip, uint16_t connecting_port,
                           const char* local_ip, uint16_t local_port, struct rist_peer* peer);
    static int connect_handler(void* arg, const char* ip, uint16_t port, const char* local_ip,
                             uint16_t local_port, struct rist_peer* peer);
    static int disconnect_handler(void* arg, struct rist_peer* peer);
};

// RIST URL parser
class SrsRistUrl
{
private:
    std::string url_;
    std::string host_;
    int port_;
    std::string mode_;
    std::map<std::string, std::string> params_;
    
public:
    SrsRistUrl();
    virtual ~SrsRistUrl();
    
public:
    // Parse RIST URL
    srs_error_t parse(const std::string& url);
    
    // Get components
    std::string host() const { return host_; }
    int port() const { return port_; }
    std::string mode() const { return mode_; }
    std::string get_param(const std::string& key) const;
    
    // Build URL string
    std::string to_string() const;
};

#endif // SRS_PROTOCOL_RIST_HPP
