/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Your Name
 */

#include <srs_app_rist_utility.hpp>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_kernel_utility.hpp>

#include <sstream>

srs_error_t SrsRistUrlHelper::parse_url(const std::string& url, std::string& host,
                                         int& port, std::string& mode)
{
    srs_error_t err = srs_success;
    
    // Expected format: rist://host:port?mode=value
    if (url.find("rist://") != 0) {
        return srs_error_new(ERROR_RIST_PARSE, "invalid RIST URL scheme: %s", url.c_str());
    }
    
    std::string rest = url.substr(7);  // Skip "rist://"
    
    // Find query string
    size_t query_pos = rest.find('?');
    std::string hostport = (query_pos != std::string::npos) ? 
        rest.substr(0, query_pos) : rest;
    
    // Parse host:port
    size_t colon_pos = hostport.find(':');
    if (colon_pos != std::string::npos) {
        host = hostport.substr(0, colon_pos);
        port = atoi(hostport.substr(colon_pos + 1).c_str());
    } else {
        host = hostport;
        port = 1971;  // Default RIST port
    }
    
    // Parse query parameters
    if (query_pos != std::string::npos) {
        std::string query = rest.substr(query_pos + 1);
        
        // Simple parameter parsing
        std::vector<std::string> params;
        srs_string_split(query, "&", params);
        
        for (size_t i = 0; i < params.size(); i++) {
            size_t eq_pos = params[i].find('=');
            if (eq_pos != std::string::npos) {
                std::string key = params[i].substr(0, eq_pos);
                std::string value = params[i].substr(eq_pos + 1);
                
                if (key == "mode") {
                    mode = value;
                }
            }
        }
    }
    
    return err;
}

std::string SrsRistUrlHelper::build_url(const std::string& host, int port,
                                        const std::string& mode)
{
    std::ostringstream ss;
    ss << "rist://" << host << ":" << port;
    
    if (!mode.empty()) {
        ss << "?mode=" << mode;
    }
    
    return ss.str();
}

srs_error_t SrsRistUrlHelper::parse_streamid(const std::string& streamid,
                                             std::string& vhost, std::string& app,
                                             std::string& stream, std::string& mode)
{
    srs_error_t err = srs_success;
    
    // Parse YAML-style streamid: #!::r=vhost/app/stream,m=publish
    if (streamid.empty()) {
        return srs_error_new(ERROR_RIST_PARSE, "empty streamid");
    }
    
    // Remove #!:: prefix if present
    std::string sid = streamid;
    if (sid.find("#!::") == 0) {
        sid = sid.substr(4);
    }
    
    // Parse key-value pairs separated by comma
    std::vector<std::string> pairs;
    srs_string_split(sid, ",", pairs);
    
    for (size_t i = 0; i < pairs.size(); i++) {
        size_t eq_pos = pairs[i].find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pairs[i].substr(0, eq_pos);
            std::string value = pairs[i].substr(eq_pos + 1);
            
            if (key == "r") {
                // Parse resource: vhost/app/stream or app/stream
                std::vector<std::string> parts;
                srs_string_split(value, "/", parts);
                
                if (parts.size() == 3) {
                    vhost = parts[0];
                    app = parts[1];
                    stream = parts[2];
                } else if (parts.size() == 2) {
                    vhost = RTMP_VHOST_DEFAULT;
                    app = parts[0];
                    stream = parts[1];
                } else if (parts.size() == 1) {
                    vhost = RTMP_VHOST_DEFAULT;
                    app = "live";
                    stream = parts[0];
                }
            } else if (key == "m") {
                mode = value;
            }
        }
    }
    
    return err;
}

std::string SrsRistUrlHelper::build_streamid(const std::string& vhost,
                                             const std::string& app,
                                             const std::string& stream,
                                             const std::string& mode)
{
    std::ostringstream ss;
    ss << "#!::r=";
    
    if (vhost != RTMP_VHOST_DEFAULT) {
        ss << vhost << "/";
    }
    
    ss << app << "/" << stream;
    
    if (!mode.empty()) {
        ss << ",m=" << mode;
    }
    
    return ss.str();
}

SrsRistStats::SrsRistStats()
{
    reset();
}

SrsRistStats::~SrsRistStats()
{
}

void SrsRistStats::reset()
{
    bytes_sent = 0;
    bytes_received = 0;
    packets_sent = 0;
    packets_received = 0;
    packets_lost = 0;
    packets_retransmitted = 0;
    rtt = 0.0;
}
