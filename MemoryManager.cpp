#include <stdio.h>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <vector>
#include <assert.h>

using namespace std;

struct Block {
  int off, sz;
  bool free;
};

class MemoryManager {
  const int n;
  set<pair<int,int>> free_sz_off;
  unordered_map<int,Block> off_block_mp;
  unordered_map<int,Block> end_block_mp;

  void add(const Block b) {
    off_block_mp.insert({b.off, b});
    end_block_mp.insert({b.off+b.sz, b});
    if (b.free) free_sz_off.insert({b.sz,b.off});
  }
  void rem(const Block b) {
    off_block_mp.erase(b.off);
    end_block_mp.erase(b.off+b.sz);
    if (b.free) free_sz_off.erase({b.sz,b.off});
  }
  Block merge(const Block left, const Block right) {
    this->rem(left);
    this->rem(right);
    const Block b = {left.off,left.sz + right.sz, true}; // TODO:
    this->add(b);
    return b;
  }
  Block maybe_merge_left(const Block b) {
    auto it = end_block_mp.find(b.off);
    if (it != end_block_mp.end() && it->second.free) {
      const auto [_, leftb] = *it;
      return merge(leftb, b);
    } else return b;
  }
  Block maybe_merge_right(const Block b) {
    auto it = off_block_mp.find(b.off+b.sz);
    if (it != off_block_mp.end() && it->second.free) {
      const auto [_, rightb] = *it;
      return merge(b, rightb);
    } else return b;
  }
public:
  MemoryManager(const int n_p) : n(n_p) {
    this->add({0,n,true});
  }
  int malloc(const int req_sz) {
    auto it = free_sz_off.lower_bound({req_sz,n+1});
    if (it == free_sz_off.end()) return -1;
    else {
      const auto [sz, off] = *it;
      this->rem(off_block_mp[off]);
      this->add({off, req_sz, false});
      if (sz > req_sz) this->add({off + req_sz, sz - req_sz, true});
      return off;
    }
  }
  int free(const int req_off) {
    auto it = off_block_mp.find(req_off);
    if (it == off_block_mp.end() || it->second.free) return -1;
    else {
      auto [_, b] = *it;
      this->rem(b);
      b.free = true; this->add(b);
      maybe_merge_right(maybe_merge_left(b));
      return 0;
    }
  }
};

int main(){
  MemoryManager m(1000);
  std::vector<int> many;
  for (int i = 0; i < 100; i++) {
    many.push_back(m.malloc(5));
  }
  for (int i = 50; i < 100; i++) {
    assert(m.free(many[i]) == 0);
  }
  for (int i = 0; i < 50; i++) {
    m.free(many[i]);
  }

  int a = m.malloc(200);
  int b = m.malloc(500);
  printf("a=%d, b=%d\n", a, b);
  return 0;
}
