#include <algorithm>
#ifdef __cplusplus
#define export exports
extern "C" {
#include "qbe/all.h"
}
#undef export
#else
#include "qbe/all.h"
#endif
#include <stdio.h>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <stack>

#ifdef DEBUG
#define DBG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DBG_PRINT(...) do {} while(0)
#endif

using Edge = std::pair<Blk*, Blk*>;

template<typename T>
using Set = std::unordered_set<T>;

/**
 * Для того чтобы хэш для unordered_set от Edge работал
 */
template<>
struct std::hash<Edge> {
    size_t operator()(const Edge& e) const noexcept {
        return std::hash<Blk*>()(e.first) ^ (std::hash<Blk*>()(e.second) << 1);
    }
};

template<>
struct std::hash<std::pair<const Blk *, int>> {
    size_t operator()(const std::pair<const Blk *, int> &pair) const noexcept {
        return std::hash<const Blk *>()(pair.first) ^ pair.second;
    }
};

/** @author ermolovich
 *  @brief  Поиск обратных дуг перебором всех дуг ГПУ функции fn
 *  @param  fn Указатель на структуру обрабатываемой функции
 *  @return Множество всех обратных дуг ГПУ функции fn
 */
Set<Edge> find_back_edges(Fn *fn) {
    Set<Edge> set = {};

    for (Blk *b = fn->start; b; b = b->link) {
        int fl = 0;
        Blk *c = b->s1;
        Blk *tmp = b;

        if(b->s1 == b || b->s2 == b) {
            set.insert(Edge(b, b));
        }

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

/** @author ermolovich
 *  @brief  Построение множества блоков естественного цикла по вектору его обратных дуг
 *  @param  edges Вектор обратных дуг, входящих в один заголовок цикла
 *  @return Множество всех базовых блоков цикла
 */
Set<Blk*> fill_loop_set(std::vector<Edge> edges) {
    // if several back edges are pointing to one header,
    // all of them are inside the same loop

    Set<Blk*> loop;

    for (int i = 0; i < edges.size(); i++) {
        std::stack<Blk*> s;
        s.push(edges[i].first);
        loop.insert(edges[i].second);

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
    }

    return loop;
}

/**
 *  @brief  Нахождение всех циклов функции fn
 *  @param  fn Указатель на структуру обрабатываемой функции
 *  @param  be Множество всех обратных дуг ГПУ функции fn
 *  @return Вектор пар из заголовков всех циклов и множеств базовых блоков
 *          соответствующих циклов функции fn
 */
std::vector<std::pair<Blk*, Set<Blk*>>> fill_loops(Fn *fn, Set<Edge> be) {
    std::vector<std::pair<Blk*, Set<Blk*>>> loops;
    auto iter = be.begin();

      while (!be.empty()) {
        // Получаем первую обратную дугу и удаляем её из множества
        Edge first = *be.begin();
        be.erase(be.begin());

        Blk *cur_h = first.second;
        std::vector<Edge> cur_loop_be = {first};

        // Ищем все остальные обратные дуги с тем же заголовком и удаляем их из множества
        for (auto it = be.begin(); it != be.end(); ) {
            if (it->second == cur_h) {
                cur_loop_be.push_back(*it);
                it = be.erase(it);  // безопасное удаление с сохранением итерации
            } else {
                ++it;
            }
        }

        loops.emplace_back(cur_h, fill_loop_set(cur_loop_be));
    }

    return loops;
}

void print_instruction(const Ins &ins) {
    char *rtypes[2] = {
        "RTmp",
        "RCon",
    };
    DBG_PRINT("(%s) %u = %s (%s) %u, (%s) %u\n", rtypes[ins.to.type], ins.to.val, optab[ins.op].name, rtypes[ins.arg[0].type], ins.arg[0].val, rtypes[ins.arg[1].type], ins.arg[1].val);
}

bool cmp_tmp_names(const char *s1, const char *s2) {
    int ind = 0;
    while (ind < NString && s1[ind] && s2[ind]) {
        if (s1[ind] == '.' && s2[ind] == '.') {
            return true;
        }
        if (s1[ind] != s2[ind]) {
            return false;
        }
        ind += 1;
    }
    return true;
}

bool inline cmp_vals(const Fn &fn, const uint ref_val_1, const uint ref_val_2) {
    return cmp_tmp_names(fn.tmp[ref_val_1].name, fn.tmp[ref_val_2].name);
}

bool is_in_invariants(const Fn &fn, const Set<uint> &invariants, const uint invariant_to_check) {
    for (const uint invariant : invariants) {
        // DBG_PRINT("Names: %s and %s\n", fn.tmp[invariant].name, fn.tmp[reference.val].name);
        if (invariant_to_check == invariant) {
            return true;
        }
    }
    return false;
}

bool is_defined_out_of_loop(const Fn &fn, const Set<Blk *> &loop, const uint ref_to_check) {
    for (const Blk *blk : loop) {
        for (int i = 0; i < blk->nins; i++) {
            if (cmp_vals(fn, ref_to_check, blk->ins[i].to.val)) {
                return false;
            }
        }
    }
    return true;
}

/** @author zvancov.mu
 *  @brief  Является ли операнд инвариантным
 *  @param  reference Операнд
 *  @param  invariants Известное множество инвариантов
 *  @param  loop Множество базовых блоков цикла
 */
bool is_invariant_reference(const Fn &fn, const Ref reference, Set<uint> &invariants, const Set<Blk *> &loop) {
    DBG_PRINT("in is_invariant_refernce\n");
    if (reference.type == RCon) return true;  // constant
    if (reference.type == RTmp) {  // variable
        if (reference.val == 0) return true;  // special value
        if (invariants.find(reference.val) != invariants.end()) {
            return true;
        }

        // next 2 for need to be ORed (||) to be done.

        if (!is_in_invariants(fn, invariants, reference.val) && !is_defined_out_of_loop(fn, loop, reference.val)) {
            goto variant_reference;
        }
        // invariants.emplace(reference.val);
        DBG_PRINT("true invariant reference\n");
        return true;
    }
variant_reference:
    return false;
}

/** @author zvancov.mu
 *  @brief  Является ли инструкция инвариантной
 *  @param  ins Указатель на инструкцию
 *  @param  invariants Известное множество инвариантов
 */
bool is_invariant_instruction(const Fn &fn, const Ins &ins, Set<uint> &invariants, const Set<Blk *> &loop, const Blk *exit) {
    DBG_PRINT("in is_invariant_instruction\n");
    Ref arg_1 = ins.arg[0], arg_2 = ins.arg[1], result = ins.to;

     for (const Blk *blk : loop) {  // only definition that dominates all exits
        for (int i = 0; i < blk->nins; i++) {
            Ins cur_ins = blk->ins[i];
            DBG_PRINT("Comparisons: %d %d %d %d %d\n", req(cur_ins.arg[0], ins.arg[0]), req(cur_ins.arg[1], ins.arg[1]), cur_ins.cls == ins.cls, cur_ins.op == ins.op, req(cur_ins.to, ins.to));
            if (req(cur_ins.arg[0], ins.arg[0]) && req(cur_ins.arg[1], ins.arg[1]) && cur_ins.cls == ins.cls && cur_ins.op == ins.op && req(cur_ins.to, ins.to)) {  // that definition
                DBG_PRINT("in that definition\n");
                Blk *dom = (Blk *) exit;
                bool flag = false;
                while (dom != nullptr) {  // dominates all exits
                    DBG_PRINT("Dom name: %s\n", dom->name);
                    if (dom == blk) {
                        flag = true;
                        break;
                    }
                    dom = dom->idom;
                }
                if (!flag) {
                    DBG_PRINT("bad instruction\n");
                    goto variant_instruction;
                }
            } else if (cur_ins.to.val == result.val) {  // Not only definition
                DBG_PRINT("not only definition\n");
                goto variant_instruction;
            } else if (is_invariant_reference(fn, arg_1, invariants, loop) && is_invariant_reference(fn, arg_2, invariants, loop)) {  // single SSA definition if it's invariant
                invariants.emplace((uint) result.val);
                return true;
            }
        }
    }
variant_instruction:
    DBG_PRINT("only definition that dominates all exits\n");
    if (is_invariant_reference(fn, arg_1, invariants, loop) && is_invariant_reference(fn, arg_2, invariants, loop)) {
        invariants.emplace((uint) result.val);
        return true;
    }
    return false;
}

/** @author zvancov.mu
 *  @brief  Найти блок выхода. Предполагается наличие ровно одного выхода
 *  @param  entry Указатель базовый блок вход
 *  @param  loop Множество базовых блоков цикла
 */
const Blk * find_exit(const Blk *entry, const Set<Blk *> &loop) {
    for (const Blk *blk : loop) {
        if (blk->s1 == entry || blk->s2 == entry) {
            return blk;
        }
    }
    die("Loop must has an exit");
}

/**
 * @brief safe preheader name construction, takes less than 24 chars 
 * @param dst array of preheader name's chars
 * @param src array of successor name's chars
*/
void fill_preheader_name(char dst[32], char src[32]){
    char tmp = src[23];
    src[23] = '\0';
    sprintf(dst, "prehead@%s", src);
    src[23] = tmp;
}

/**
 *  @brief  Вставка предзаголовка перед циклом loop с заголовком h функции fn
 *  @param  fn   Указатель на структуру обрабатываемой функции
 *  @param  h    Указатель на базовый блок заголовка цикла
 *  @param  loop Множество базовых блоков функции fn
 *  @return pointer to the preheader
 */
Blk *insert_preheader(Fn *fn, Blk *h, Set<Blk*> &loop) {
    Blk *ph = blknew();
    fill_preheader_name(ph->name, h->name);
    fn->nblk++;

    // Присоединим выход ph в h
    ph->s1 = h;
    ph->link = h;
    ph->jmp.arg = R;
    ph->jmp.type = Jjmp;

    // Теперь предки h станут предками ph, а у h в предках будет только ph
    ph->pred = h->pred;
    ph->npred = h->npred;
    h->pred = (Blk **)alloc(sizeof(Blk *) * 1);
    h->pred[0] = ph;
    h->npred = 1;

    // Если h это стартовый блок, то по выйдем сразу
    if(fn->start == h){
        fn->start = ph;
        return ph;
    }
    Blk *linkPred = nullptr;
    for (Blk *b = fn->start; !!b; b = b->link) {
        if (b->s1 == h && !loop.count(b)) {
            b->s1 = ph;
        }

        if (b->s2 && b->s2 == h && !loop.count(b)) {
            b->s2 = ph;
        }

        if(b->link == h){
            linkPred = b;
        }
    }
    linkPred->link = ph;
    return ph;
}

/**
 * @brief moves marked invariant instructions to preheader
 * @param ph preheader Blk pointer
 * @param inv_instr_set consits of Blk and order of marked instruction in it
*/
void move_instructions(Blk *ph, Set<std::pair<const Blk *, int>> inv_instr_set){
    auto old_count = ph->nins;
    auto new_count = ph->nins + inv_instr_set.size();
    // alloc bigger array and copy instructions
    Ins *new_ins_arr = (Ins *)alloc(new_count * sizeof(Ins));
    memcpy(new_ins_arr, ph->ins, ph->nins);

    /// stores count of instructions to delete from each blk for prealloc
    std::unordered_map<Blk *, unsigned> blk_size_diff{};

    // append new instructions to preheader
    unsigned write_ptr = old_count;
    for (const auto &[blk, ins_order] : inv_instr_set) {
        new_ins_arr[write_ptr++] = blk->ins[ins_order];
        blk_size_diff[(Blk *)blk]++;
        DBG_PRINT("moved\n\t"); 
        print_instruction(blk->ins[ins_order]);
        DBG_PRINT("\tfrom %s to %s;\n", blk->name, ph->name);
    }

    // reduce blks size
    for(auto [blk, diff] : blk_size_diff){
        unsigned reduce_write_ptr = 0;
        auto reduced_size = blk->nins - diff;
        Ins *reduced_instr_arr = (Ins *)alloc(reduced_size * sizeof(Ins));
        // copy all except invariants
        for(int i = 0; i < blk->nins; i++){
            if (inv_instr_set.count(std::pair(blk, i))) {
                // DBG_PRINT(">>skiped "); print_instruction(blk->ins[i]);
                continue;
            }
            reduced_instr_arr[reduce_write_ptr++] = blk->ins[i];
        }

        blk->ins = reduced_instr_arr;
        blk->nins = reduced_size;
    }

    ph->ins = new_ins_arr;
    ph->nins = write_ptr;
}

/** @author zvancov.mu
 *  @brief  Движение инвариантного кода
 *  @param  fn   Указатель на структуру обрабатываемой функции
 *  @param  header    Указатель на базовый блок заголовка цикла
 *  @param  loop Множество базовых блоков цикла
 */
void loop_invariant_code_motion(Fn &fn, Blk *header, Set<Blk *> &loop) {
    Blk *loop_in = header;
    // Blk *loop_in = preheader->s1;
    Set<uint> loop_invariants;
    Set<std::pair<const Blk *, int>> invariant_instructions;
    const Blk *exit = find_exit(loop_in, loop);
    bool new_invariant;

    do {
        DBG_PRINT("in licm\n");
        new_invariant = false;
        for (const Blk *blk : loop) {
            for (int i = 0; i < blk->nins; i++) {
                const Ins ins = blk->ins[i];
                print_instruction(ins);
                if (invariant_instructions.find(std::pair(blk, i)) != invariant_instructions.end()) {
                    continue;
                }
                if (is_invariant_instruction(fn, ins, loop_invariants, loop, exit)) {
                    new_invariant = true;
                    invariant_instructions.emplace(blk, i);
                }
            }
        }
    } while (new_invariant);
    if(invariant_instructions.size() > 0){
        Blk *preheader = insert_preheader(&fn, header, loop);
        DBG_PRINT("Loop invariants:\n");
        for (auto [blk, ins_order] : invariant_instructions) {
            DBG_PRINT("\tInvariant instruction: block %s, instruction %d\n", blk->name, ins_order);
        }
        move_instructions(preheader, invariant_instructions);
    }
}

static void readfn (Fn *fn) {
    fillrpo(fn); // Traverses the CFG in reverse post-order, filling blk->id.
    fillpreds(fn);
    filluse(fn);

    // in order not to break phi functions in blocks we are skipping ssa()
    // but dominators are necessary for back edges though, so we filldom()

    filldom(fn);
    Set<Edge> back_edges = find_back_edges(fn);
    std::vector<std::pair<Blk*, Set<Blk*>>> loops = fill_loops(fn, back_edges);
    DBG_PRINT("loops: %d %d\n", loops.size(), back_edges.size());
    for (int i = 0; i < loops.size(); i++) {
        auto iter = loops[i].second.begin();

        for (; iter != loops[i].second.end(); iter++) {
            DBG_PRINT("%s ", (*iter)->name);
        }

        DBG_PRINT("\n");
    }

    fillrpo(fn);
    // fillpreds(fn);
    filluse(fn);
    ssa(fn);
    filluse(fn);
    ssacheck(fn);
    filldom(fn);
    std::sort(loops.begin(), loops.end(), [](std::pair<Blk*, Set<Blk*>> a, std::pair<Blk*, Set<Blk*>> b){ return a.second.size() > b.second.size();});
    for(auto &loop : loops) {
    // for(auto rit = loops.rbegin(); rit < loops.rend(); rit++) {
        loop_invariant_code_motion(*fn, loop.first, loop.second);
        // loop_invariant_code_motion(*fn, rit->first, rit->second);
    }

    printfn(fn, stdout);
}

static void readdat (Dat *dat) {
  (void) dat;
}

int main () {
  parse(stdin, "<stdin>", readdat, readfn);
  freeall();
}