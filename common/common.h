#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/tcp.h>
// #include <openssl/err.h>
// #include <openssl/ssl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

const int kBitLen = 23;
const int kNumMedian = 7;
const uint64_t kTieBreakerBit = 25;
const uint64_t kTieBreakerRange = (1 << kTieBreakerBit);

enum SERVER_ID { S1, S2, S3 };

#define MYASSERT(expr)                                      \
    if (!(expr)) {                                          \
        printf("FAILED at %s:%d!!!\n", __FILE__, __LINE__); \
        exit(1);                                            \
    }

using namespace std;
using namespace chrono;

vector<string> split(string &s, const string &delim) {
    vector<string> results;
    if (s == "") {
        return results;
    }
    size_t prev = 0, cur = 0;
    do {
        cur = s.find(delim, prev);
        if (cur == string::npos) {
            cur = s.length();
        }
        string part = s.substr(prev, cur - prev);
        if (!part.empty()) {
            results.emplace_back(part);
        }
        prev = cur + delim.length();
    } while (cur < s.length() && prev < s.length());
    return results;
}

struct sockaddr_in string_to_struct_addr(string &str) {
    struct sockaddr_in result;
    memset(&result, 0, sizeof(result));
    auto addr_and_port = split(str, ":");
    result.sin_addr.s_addr = inet_addr(addr_and_port[0].c_str());
    result.sin_family = AF_INET;
    result.sin_port = htons(atoi(addr_and_port[1].c_str()));
    return result;
}

// -1 indicate errrors
int connect_to_addr(string addr) {
    struct timeval Timeout;
    Timeout.tv_sec = 6000;
    Timeout.tv_usec = 0;
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sockaddr = string_to_struct_addr(addr);
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        fprintf(stderr, "Get flags error:%s\n", strerror(errno));
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        cout << "get flags failed\n";
        close(fd);
        return -1;
    }
    fd_set wait_set;
    int res = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    // connected
    if (res == 0) {
        if (fcntl(fd, F_SETFL, flags) < 0) {
            cout << "connect fcntl failed\n";
            close(fd);
            return -1;
        }
        return fd;
    } else {
        FD_ZERO(&wait_set);
        FD_SET(fd, &wait_set);

        // wait for socket to be writable; return after given timeout
        res = select(fd + 1, NULL, &wait_set, NULL, &Timeout);
        // return -1;
    }
    if (fcntl(fd, F_SETFL, flags) < 0) {
        cout << "connect fcntl failed\n";
        close(fd);
        return -1;
    }

    if (res < 0) {
        cout << "connect timeout\n";
        close(fd);
        return -1;
    }

    if (FD_ISSET(fd, &wait_set)) {
        // cout << "get a valid fd: " << fd << endl;
        return fd;
    } else {
        cout << "connect timeout\n";
        close(fd);
        return -1;
    }
    MYASSERT(0);
    return -1;
}

// bool do_write_ssl(SSL *ssl, const char *buf, uint64_t len) {
//     uint64_t sent = 0;
//     while (sent < len) {
//         int n = SSL_write(ssl, &buf[sent], len - sent);
//         if (n < 0) {
//             fprintf(stderr, "Fail to write: %s\n", strerror(errno));
//             return false;
//         }
//         sent += n;
//     }
//     return true;
// }

// bool do_read_ssl(SSL *ssl, char *buf, uint64_t len) {
//     uint64_t rcvd = 0;
//     while (rcvd < len) {
//         int n = SSL_read(ssl, &buf[rcvd], len - rcvd);
//         if (n < 0) {
//             fprintf(stderr, "Fail to read: %d %s\n", errno, strerror(errno));
//             return false;
//         }
//         rcvd += n;
//     }
//     return true;
// }

// // Changed message size type `uint64_t` as the length of message
// string formatMsg_ssl(string input) {
//     char len[sizeof(uint64_t)];
//     uint64_t str_len = htonl(input.size());
//     memcpy(len, &str_len, sizeof(uint64_t));
//     string prefix = string(len, sizeof(uint64_t));
//     prefix.append(input);
//     MYASSERT(prefix.size() == input.size() + sizeof(uint64_t));
//     return prefix;
// }

// // bool do_read_blocking(int fd, char *buf, uint64_t len) {
// //     uint64_t rcvd = 0;
// //     while (rcvd < len) {
// //         int n = read(fd, &buf[rcvd], len - rcvd);
// //         if (n < 0) {
// //             fprintf(stderr, "Fail to read: %d %s\n", errno,
// strerror(errno));
// //             return false;
// //         }
// //         rcvd += n;
// //     }
// //     return true;
// // }

// bool sendMsg_ssl(SSL *ssl, string msg) {
//     string send_msg = formatMsg_ssl(msg);
//     if (!do_write_ssl(ssl, send_msg.c_str(), send_msg.size())) {
//         return false;
//     }
//     return true;
// }

// bool recvMsg_ssl(SSL *ssl, string &res) {
//     // start receiving response
//     uint64_t res_len = 0;
//     if (do_read_ssl(ssl, (char *)&res_len, sizeof(uint64_t)) == false) {
//         cout << "receive header fail\n";
//         return false;
//     }
//     res_len = ntohl(res_len);
//     char *tmp = (char *)malloc(sizeof(char) * res_len);
//     memset(tmp, 0, res_len);
//     if (do_read_ssl(ssl, tmp, res_len) == false) {
//         cout << "receive response fail\n";
//         return false;
//     }
//     res = string(tmp, res_len);
//     free(tmp);
//     return true;
// }

/*functions for non-ssl reading*/
bool do_write(int fd, const char *buf, uint64_t len) {
    uint64_t sent = 0;
    while (sent < len) {
        int64_t n = write(fd, &buf[sent], len - sent);
        if (n < 0) {
            fprintf(stdout, "Fail to write: %s\n", strerror(errno));
            return false;
        }
        sent += static_cast<uint64_t>(n);
    }
    return true;
}

bool do_read(int fd, char *buf, uint64_t len) {
    uint64_t rcvd = 0;
    while (rcvd < len) {
        int64_t n = read(fd, &buf[rcvd], len - rcvd);
        if (n < 0) {
            fprintf(stdout, "Fail to read: %d %s\n", errno, strerror(errno));
            return false;
        }
        rcvd += static_cast<uint64_t>(n);
    }
    return true;
}

// Changed message size type `uint64_t` as the length of message
string formatMsg(string input) {
    char len[sizeof(uint64_t)];
    uint64_t str_len = htobe64(input.size());
    memcpy(len, &str_len, sizeof(uint64_t));
    string prefix = string(len, sizeof(uint64_t));
    prefix.append(input);
    MYASSERT(prefix.size() == input.size() + sizeof(uint64_t));
    return prefix;
}

bool sendMsg(int fd, string msg) {
    string send_msg = formatMsg(msg);
    if (!do_write(fd, send_msg.c_str(), send_msg.size())) {
        return false;
    }
    return true;
}

bool recvMsg(int fd, string &res) {
    // start receiving response
    uint64_t res_len = 0;
    if (do_read(fd, (char *)&res_len, sizeof(uint64_t)) == false) {
        cout << "receive header fail\n";
        return false;
    }
    res_len = be64toh(res_len);
    res = string(res_len, 0);
    if (do_read(fd, (char *)res.c_str(), res_len) == false) {
        cout << "receive response fail\n";
        return false;
    }
    return true;
}

/*common functions for parsing config*/
map<string, string> parseConfig(string config_file) {
    fstream fs;
    fs.open(config_file, fstream::in);
    map<string, string> config_map;
    while (true) {
        string param_name, param_val;
        getline(fs, param_name);
        getline(fs, param_val);
        config_map[param_name] = param_val;
        cout << "[config]: " << param_name << ": " << param_val << endl;
        if (fs.eof()) {
            break;
        }
    }
    return config_map;
}

int accessIntFromConfig(map<string, string> config_map, string config_name) {
    if (config_map.find(config_name) == config_map.end()) {
        cout << config_name << " not found\n";
        MYASSERT(0);
    }
    return atoi(config_map[config_name].c_str());
}

string accessStrFromConfig(map<string, string> config_map, string config_name) {
    if (config_map.find(config_name) == config_map.end()) {
        cout << config_name << " not found\n";
        MYASSERT(0);
    }
    return config_map[config_name];
}

uint64_t accessUint64FromConfig(map<string, string> config_map,
                                string config_name) {
    if (config_map.find(config_name) == config_map.end()) {
        cout << config_name << " not found\n";
        MYASSERT(0);
    }
    istringstream ss(config_map[config_name]);
    uint64_t val;
    ss >> val;
    cout << "parsed uint64_t val is: " << val << endl;
    return val;
}

double accessDoubleFromConfig(map<string, string> config_map,
                              string config_name) {
    if (config_map.find(config_name) == config_map.end()) {
        cout << config_name << " not found\n";
        MYASSERT(0);
    }
    istringstream ss(config_map[config_name]);
    double val;
    ss >> val;
    cout << "parsed double val is: " << val << endl;
    return val;
}

enum SERVER_ID accessServerIdFromConfig(map<string, string> config_map,
                                        string config_name) {
    int sid = accessIntFromConfig(config_map, config_name);
    if (sid == 1) {
        return SERVER_ID::S1;
    } else if (sid == 2) {
        return SERVER_ID::S2;
    } else if (sid == 3) {
        return SERVER_ID::S3;
    } else {
        MYASSERT(0);
    }
}

#endif  // COMMON_H