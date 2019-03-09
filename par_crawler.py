## BFS and producer / consumer based multithreaded crawler

import threading
from collections import deque
from bs4 import BeautifulSoup
import urllib.request
import re

def scrape(url):
    html_page = urllib.request.urlopen(url)
    soup = BeautifulSoup(html_page,features="html.parser")
    url_list = []
    domain = urllib.parse.urlparse(url).netloc
    for link in soup.findAll('a'):
        u = link.get('href')
        if u == None:
            continue
        u = urllib.parse.urljoin(url, u)
        #print(u)
        if urllib.parse.urlparse(u).netloc != domain:
            continue
        url_list.append(u)
    return url_list

class ParCrawler(object):
    def __init__(self, maxThreads=2):
        self.object_lock = threading.Lock()
        self.cv_empty = threading.Condition(self.object_lock)
        self.cv_full = threading.Condition(self.object_lock)
        self.maxThreads = maxThreads
        self.numAliveThreads = 0

    def consumer(self, url, q, visited):
        #print("Starting thread: %s %d" % (url, self.numAliveThreads))
        assert(self.numAliveThreads <= self.maxThreads)
        try:
            url_list = scrape(url)
        except Exception as e:
            print("Error scraping url %s: error = %s" % (url, e))
            url_list = []

        with self.object_lock:
            ## Make: 'q' grow and  'numAliveThreads' shrink
            ## Notify producers
            for u in url_list:
                if not u in visited:
                    visited.add(u)
                    q.append(u)
            self.numAliveThreads -= 1
            self.cv_full.notify_all()
            if self.numAliveThreads == 0 or len(q) == 0:
                self.cv_empty.notify_all()
        #print("Ending thread: %s" % url)

    def crawl(self, url='', maxThreads = 10):
        visited = set()
        q = deque()
        visited.add(url)
        q.append(url)
        ret = []
        while True:
            with self.object_lock:
                ## Wait for: 'q' to grow or 'numAliveThreads' to shrink to zero
                ## Make : 'q' shrink
                self.cv_empty.wait_for(
                    lambda: len(q) != 0 or self.numAliveThreads == 0
                )
                if len(q) == 0:
                    break
                u = q.popleft()
                ret.append(u)
            # Cap max pages scraped, for sanity
            if len(ret) > 30:
                break
            with self.object_lock:
                ## Wait for : 'numAliveThreads' to shrink below full
                ## Make : 'numAliveThreads' grow and launch consumer
                self.cv_full.wait_for(lambda: self.numAliveThreads < self.maxThreads)
                self.numAliveThreads += 1
                threading.Thread(
                    target=self.consumer,
                    args=(u, q, visited)
                ).start()
        return ret


from concurrent.futures import ThreadPoolExecutor
def multi_crawl(pc = ParCrawler(maxThreads=4)):
    with ThreadPoolExecutor(max_workers = 2) as e:
        fut1 = e.submit(pc.crawl, 'https://www.openai.com')
        fut2 = e.submit(pc.crawl, 'https://news.ycombinator.com/')

        print(fut1.result())
        print(fut2.result())

if __name__ == "__main__":
    multi_crawl()
