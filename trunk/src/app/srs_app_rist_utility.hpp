/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 */

#ifndef SRS_APP_RIST_UTILITY_HPP
#define SRS_APP_RIST_UTILITY_HPP

#include <srs_core.hpp>
#include <string>

// RIST URL utilities
class SrsRistUrlHelper
{
public:
    // Parse RIST URL and extract components
    static srs_error_t parse_url(const std::string& url, std::string& host, 
                                 int& port, std::string& mode);
    
    // Build RIST URL from components
    static std::string build_url(const std::string& host, int port, 
                                 const std::string& mode);
    
    // Parse streamid parameter (YAML format)
    static srs_error_t parse_streamid(const std::string& streamid,
                                      std::string& vhost, std::string& app,
                                      std::string& stream, std::string& mode);
    
    // Build streamid parameter
    static std::string build_streamid(const std::string& vhost,
                                     const std::string& app,
                                     const std::string& stream,
                                     const std::string& mode);
};

// RIST statistics utilities
class SrsRistStats
{
public:
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t packets_lost;
    uint64_t packets_retransmitted;
    double rtt;
    
public:
    SrsRistStats();
    virtual ~SrsRistStats();
    
    void reset();
};

#endif // SRS_APP_RIST_UTILITY_HPP
