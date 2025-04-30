#ifdef __cplusplus
#define export exports
extern "C" {
#include "all.h"
}
#undef export
#else
#include <qbe/all.h>
#endif
#include <stdio.h>
#include <vector>
#include <set>
#include <stack>

typedef std::pair<Blk*, Blk*> Edge;

// if several back edges are pointing to one header,
// all of them are inside the same loop

std::set<Edge> find_back_edges(Fn *fn) {
    std::set<Edge> set = {};
    Blk *b;

    for (b = fn->start; b; b = b->link) {
        int fl = 0;
        Blk *c = b->s1;
        Blk *tmp = b;

        while (c && !fl && tmp != fn->start) {
            tmp = tmp->idom;

            if (tmp == c) {
                fl = 1;
            }
        }

        if (fl) {
            set.insert(Edge(b, c));
        }

        c = b->s2;
        tmp = b;
        fl = 0;

        while (c && !fl && tmp != fn->start) {
            tmp = tmp->idom;

            if (tmp == c) {
                fl = 1;
            }
        }

        if (fl) {
            set.insert(Edge(b, c));
        }
    }

    return set;
}

std::set<Blk*> fill_loop_set(Edge edge) {
    std::set<Blk*> loop;
    std::stack<Blk*> s;
    s.push(edge.first);
    loop.insert(edge.second);

    while (!s.empty()) {
        Blk *cur = s.top();
        s.pop();

        if (loop.find(cur) == loop.end()) {
            loop.insert(cur);

            for (int i = 0; i < cur->npred; i++) {
                s.push(cur->pred[i]);
            }
        }
    }

    return loop;
}

void merge_latches(Fn *fn, std::vector<Edge> &be) {
    // create blk with exactly one unconditional jump to loop header
    // replace in all latches header jump to new block jump
    // update all metadata-struct fields with new cfg configuration
}

std::vector<std::set<Blk*>> fill_loops(Fn *fn, std::set<Edge> be) {
    std::vector<std::set<Blk*>> loops;
    auto iter = be.begin();

    while (iter != be.end()) {
        std::vector<Edge> cur_loop_be;
        cur_loop_be.push_back(*iter);
        iter = be.erase(iter);

        // check remaining vector elements for back edges of the same loop,
        // delete them from vector and fill loop block set

        auto i = iter;

        while (i != be.end()) {
            if (i->second == cur_loop_be[0].second) {
                cur_loop_be.push_back(*i);
                i = be.erase(i);
            } else {
                i++;
            }
        }

        if (cur_loop_be.size() != 1) {
            merge_latches(fn, cur_loop_be);
        }

        loops.push_back(fill_loop_set(cur_loop_be[0]));
    }

    return loops;
}

static void readfn (Fn *fn) {
    fillrpo(fn); // Traverses the CFG in reverse post-order, filling blk->id.
    fillpreds(fn);
    filluse(fn);
    ssa(fn);

    std::set<Edge> back_edges = find_back_edges(fn);
    // auto iter = back_edges.begin();

    // for (; iter != back_edges.end(); iter++) {
    //     printf("back edge: %s %s\n", iter->first->name, iter->second->name);
    //     auto loop = fill_loop_set(*iter);

    //     printf("loop elements: \n");
    //     for (auto elem = loop.begin(); elem != loop.end(); elem++) {
    //         printf("%s ", (*elem)->name);
    //     }

    //     printf("\n");
    // }

    printfn(fn, stdout);
}

static void readdat (Dat *dat) {
  (void) dat;
}

int main () {
  parse(stdin, "<stdin>", readdat, readfn);
  freeall();
}
