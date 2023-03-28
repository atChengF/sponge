#include "byte_stream.hh"
#include "socket.hh"
#include "util.hh"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>
#include<unistd.h>

using namespace std;

/*hello world!*/
void get_URL(const string &host, const string &path) {

    // Your code here.
    // You will need to connect to the "http" service on
    // the computer whose name is in the "host" string,
    // then request the URL path given in the "path" string.
    fork();
   /* msghdr a;
    recvmsg();
    sendmmsg()*/

    TCPSocket socket {};
    Address address(host, "http");
    std::string HTTPRequest =   "GET " + path + " HTTP/1.1\r\n" +       // row 1
                              "Host: " + host + "\r\n" +              // row 2
                              "Connection: close\r\n"  +              // row 3
                              "\r\n";
    socket.connect(address);
    socket.write(HTTPRequest,true);
    std::string ans;
    while(!socket.eof()){
        ans = socket.read();
        std::cout << ans;
    }
    socket.close();
    // Then you'll need to print out everything the server sends back,
    // (not just one call to read() -- everything) until you reach
    // the "eof" (end of file).
}
/*bool cmp(int a, int b){
    return a - b >= 0;
}*/
/*
 * 初始化 unordered_map<int, int> map;
 * 对 key 进行遍历
 * for(auto i = map.begin(); i != map.end(); i ++ ) i 为指向 pair<key, value>的指针
 * for(auto & i : map) i 为 pair<key, value> 的引用
 * 赋值 map[0] = 1;
 * 判断key 是否存在 map.count(key) == 1
 * map.eraser(key) 删除
 * map.clear()
 * */

/*
 * sort() 用法 左闭，右开
 * a[8]
 * 排序数组时候，里面放置地址
 * sort(a, a + 8);
 * vector<int> v
 * sort(v.begin(), v.end());
 *
 * sort( , , cmp);
 * 可以重写比较器
 *
 * */

/*
 * lower_bound() 用法
 *
 * */
/*int main(){
    ByteStream byte_stream{3};
    byte_stream.write("abcdef");
    byte_stream.pop_output(1);
    cout << byte_stream.remaining_capacity();
    cout << byte_stream.buffer_size();
    byte_stream.write("abc");
    cout << byte_stream.peek_output(3);
    return  0;
}*/


int main(int argc, char *argv[]) {
    std::string s = "dasfa";
    char* p = &s[0];
    cout << p;
    cout << p + 1;
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];
        cout << host;
        cout << path;

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;

}
