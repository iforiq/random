// Compile: g++ -std=c++0x par_crawler.cpp -lcurl

#include <stdio.h>

#include <unordered_set>
#include <deque>
#include <vector>
#include <string>

#include <chrono>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>


using namespace std;

class ParCrawler {
    condition_variable cv_empty, cv_full;
    mutex m; // object level mutex
    int numAliveThreads;
    const int maxThreads;

    // TOY Scraping function
    void scrape(const string& url, vector<string>* ret) {
        const int n = url.length();
        ret->push_back(url.substr(0, n/2) + "/abc");
        ret->push_back(url.substr(n/2, n) + "/abc");
        ret->push_back(url.substr(0, n/4) + "/xyz");
        ret->push_back(url.substr(0, n/8) + "/pqr");
        this_thread::sleep_for(std::chrono::milliseconds(100));
    }


    void consumer(const string& url,
                  deque<string>* q,
                  unordered_set<string>* seen) {
        //printf("Starting thread: %s\n", url.c_str());
        assert(numAliveThreads <= maxThreads);
        auto checkPush = [q, seen](const string& s) {
            if (seen->find(s) == seen->end()) {
                seen->insert(s);
                q->push_front(s);
            }
        };
        vector<string> urlList;
        try {
            scrape(url, &urlList);
        } catch(...) {
            ;
        }

        {
            // Make: 'q' grow and  'numAliveThreads' shrink
            // Notify producers
            unique_lock<mutex> lk(m);
            for (const string& s: urlList) checkPush(s);
            --numAliveThreads;
            cv_full.notify_all();
            if (!q->empty() || numAliveThreads == 0) cv_empty.notify_all();
        }
        //printf("Ending thread: %s\n", url.c_str());
    }

public:
    ParCrawler (const int nt) : maxThreads(nt) {
        numAliveThreads = 0;
    }

    vector<string> crawl(const string& url) {
        deque<string> q;  // protect access via object level lock
        unordered_set<string> seen; // protect access via object level lock
        q.push_front(url);
        seen.insert(url);
        vector<string> ret;
        while(true) {
            {
                // Wait for: 'q' to grow or 'numAliveThreads' to shrink to zero
                // Make : 'q' shrink
                unique_lock<mutex> lk(m);
                cv_empty.wait(lk,
                              [&]() { return !q.empty() || numAliveThreads == 0;});
                if (q.empty()) break;
                ret.push_back(q.front());
                q.pop_front();
            }
            {
                // Wait for : 'numAliveThreads' to shrink below full
                // Make : 'numAliveThreads' grow and launch consumer
                unique_lock<mutex> lk(m);
                cv_full.wait(lk,
                             [&]() { return numAliveThreads < maxThreads; });
                ++numAliveThreads;
                thread(&ParCrawler::consumer, this, ret.back(), &q, &seen).detach();
            }
        }
        return ret;
    }
};

int main() {
    ParCrawler pc(10);
    auto fut1 = async(launch::async, &ParCrawler::crawl, &pc,
                      "https://www.openai.com/");
    auto fut2 = async(launch::async, &ParCrawler::crawl, &pc,
                      "https://news.ycombinator.com/");

    for (auto& s : fut1.get()) printf("%s, ", s.c_str());
    printf("\n\n");
    for (auto& s : fut2.get()) printf("%s, ", s.c_str());
    printf("\n\n");
    return 0;
}
