/*
 scns.hpp
 Katsuki Ohto
 */

// sibling conspiracy number search

#ifndef SHOGI_RAPUNZEL_SCNS_HPP_
#define SHOGI_RAPUNZEL_SCNS_HPP_

#include "../../shogi.h"
#include "../../extra/all.h"

#include "common/common_all.h"

double kDelta = 0.187;

// 記録
int gMoveCount;
int gMaxDepth;
Color gRootColor;
ClockMS gClock;

constexpr int N_VALUE_STAIRS = 64;

int valueToStair(int value){
    return value / 50;
}

int clip(int e){
    return min(N_VALUE_STAIRS - 1, max(-N_VALUE_STAIRS + 1, e));
}

constexpr int VALUE_INF = N_VALUE_STAIRS;
constexpr int PD_INF = 1000000;

// 証明数と反証数関数の初期値(評価点からの計算)
/*int pInit(const int x, int e){
    return (e <= x) ? 0
    : 1;
}
int dInit(const int x, int e){
    return (e >= x) ? 0
    : 1;
}*/

int pInit(const int x, int e){
    return (e <= x) ? 0
    : int(pow(pow(2, e - x), kDelta));
}
int dInit(const int x, int e){
    return (e >= x) ? 0
    : int(pow(pow(2, x - e), kDelta));
}


// 評価点を保持して証明数と反証数を計算するためのクラス
struct ValueNumberList{
    std::array<int, N_VALUE_STAIRS * 2 + 1> number;
    int minimaxValue;
    
    ValueNumberList():
    number(), minimaxValue(0){}
    
    /*ValueList(const std::vector<T>& avalue, std::size_t amid):
    value(avalue), mid(amid){
        
    }*/
    
    void init(int x)noexcept{
        minimaxValue = x;
        atValue(x) = 0;
        for(int e = -VALUE_INF; e < x; ++e){
            atValue(e) = dInit(x, e);
        }
        for(int e = x + 1; e <= VALUE_INF; ++e){
            atValue(e) = pInit(x, e);
        }
    }
    
    void initTerminal(int x)noexcept{
        minimaxValue = x;
        for(int e = -VALUE_INF; e <= x; ++e){
            atValue(e) = 0;
        }
        for(int e = x + 1; e <= VALUE_INF; ++e){
            atValue(e) = PD_INF;
        }
    }
    
    int& at(int index){ return number[index]; }
    int at(int index)const{ return number[index]; }
    
    int& atValue(int v){ return number[v + N_VALUE_STAIRS]; }
    int atValue(int v)const{ return number[v + N_VALUE_STAIRS]; }
    
    std::string toString()const{
        std::ostringstream oss;
        oss << "[";
        for(int e = -VALUE_INF; e <= VALUE_INF; ++e){
            oss << atValue(e) << ", ";
        }
        oss << "]";
        oss << " minimax = " << minimaxValue;
        return oss.str();
    }
    
    bool exam()const{
        if(!(-VALUE_INF <= minimaxValue && minimaxValue <= VALUE_INF)){
            cerr << "invalid minimax value" << endl;
            return false;
        }
        for(int e = -VALUE_INF; e < minimaxValue; ++e){
            if(atValue(e) < atValue(e + 1)){
                cerr << "inmonotonic lower values" << endl;
                return false;
            }
        }
        for(int e = minimaxValue; e < VALUE_INF; ++e){
            if(atValue(e) > atValue(e + 1)){
                cerr << "inmonotonic upper values" << endl;
                return false;
            }
        }
        return true;
    }
};

std::ostream& operator<<(std::ostream& ost, const ValueNumberList& valueList){
    ost << valueList.toString();
    return ost;
}

// 証明数と反証数の計算
template<class value_list_t>
int pn(const value_list_t& v, int e){
    if(e <= v.minimaxValue){
        return 0;
    }else{
        return v.atValue(e);
    }
}
template<class value_list_t>
int dn(const value_list_t& v, int e){
    if(e >= v.minimaxValue){
        return 0;
    }else{
        return v.atValue(e);
    }
}

// 証明数、反証数閾値からの評価点ウィンドウ計算（定義通り）
template<class value_list_t>
int vminByDef(const value_list_t& v, int d){
    if(dn(v, -VALUE_INF) <= d){
        return -VALUE_INF;
    }
    for(int e = -VALUE_INF; e <= VALUE_INF; ++e){
        if(dn(v, e) <= d){
            return e;
        }
    }
    return -VALUE_INF;
}

template<class value_list_t>
int vmaxByDef(const value_list_t& v, int p){
    if(pn(v, VALUE_INF) <= p){
        return VALUE_INF;
    }
    for(int e = VALUE_INF; e >= VALUE_INF; --e){
        if(pn(v, e) <= p){
            return e;
        }
    }
    return VALUE_INF;
}

// 証明数、反証数閾値からの評価点ウィンドウ計算（高速版）
/*template<class value_list_t>
double vmin(const value_list_t& v, int d){
    return v.lowerSize() < d ? -DBL_MAX : v.atLower(d);
}
template<class value_list_t>
double vmax(const value_list_t& v, int p){
    return v.upperSize() < p ? DBL_MAX : v.atUpper(p);
}*/

template<class board_t>
struct Node{
    
    double e; // 最後の着手の相対評価
    
    Move move; // 最後の着手
    
    int x; // ルートからの相対評価値
    
    // 証明数、反証数関数のための評価値一覧
    ValueNumberList vList;
    
    std::vector<Node> child; // 子ノード
    
    Node *parent; // 親
    
    board_t pos;
    
    int depth;
    
    int count; // 探索回数
    
    bool terminal;
    
    Node(const board_t& apos){ // ルートノードの初期化
        pos = apos;
        parent = nullptr;
        move = MOVE_RESIGN;
        depth = 0;
        count = 0;
        x = 0;
        terminal = false;
    }
    
    Node(const board_t& apos, Node& aparent, Move amv){ // ルート以外のノードの初期化
        pos = apos;
        parent = &aparent;
        move = amv;
        depth = aparent.depth + 1;
        count = 0;
        gMaxDepth = max(gMaxDepth, depth);
        terminal = false;
    }
    
    bool isLeaf()const noexcept{
        return !child.size();
    }
    Color turnColor()const noexcept{
        return pos.side_to_move();
    }
    bool isMaxNode()const noexcept{
        return gRootColor == turnColor();
    }
    
    // 証明数、反証数関数
    int p(double v){
        if(isLeaf()){
            return pInit(x, v);
        }else{
            return pn(vList, v);
        }
    }
    int d(double v){
        if(isLeaf()){
            return dInit(x, v);
        }else{
            return dn(vList, v);
        }
    }
    
    void expand(){
        
        MoveList<LEGAL_ALL> moves(pos); // 着手生成
        int legalMoves = 0;
        
        if(moves.size() == 0){ // terminal nodeであることが発覚した
            
            terminal = true;
            vList.initTerminal(x);
            assert(vList.exam());
            
        }else{
            
            child.clear();
            
            //double bestE = -999999;
            int bestE = -VALUE_INF;
            
            DERR << "last move = " << move << endl;
            DERR << pos;
            
            StateInfo si;
            for(const auto mv : moves){
                
                DERR << mv;
                
                Square to = move_to(mv);
                Piece to_pc = pos.piece_on(to);
                if(to_pc == B_KING || to_pc == W_KING){
                    if(pos.side_to_move() == color_of(to_pc)){
                        continue;
                    }else{
                        // 相手の王を取れているので勝ち...
                        continue;
                        //vList.initTerminal(pos.side_to_move() == BLACK);
                    }
                }
                
                if(!pos.pseudo_legal(mv) || !pos.legal(mv)){
                    DERR << " -> illegal!" << endl;
                    continue;
                }
                
                ++legalMoves;
                
                DERR << endl;
                
                // 局面を進める
                
                pos.do_move(mv, si);
                ++gMoveCount;
                
                // 下位ノードの情報を初期化
                Node node(pos, *this, mv);
                //node.pos.check_info_update();
                node.pos.template set_check_info<false>(&si);
                
                // 全ての着手の評価値を見るまでは行動評価値を正の値に変換して置いておく
                //double E = pow(1.01, static_cast<double>(evaluate(node.pos)));
                int E = valueToStair(Eval::evaluate(node.pos));
                bestE = max(bestE, E);
                node.e = E;
                
                child.emplace_back(node);
                
                // 局面を戻す
                pos.undo_move(mv);
            }
            
            if(legalMoves){
                
                // 各ノードの評価値を相対エラー値に変換
                /*for(auto& ch : child){
                 ch.e = log(bestE / ch.e);
                 }*/
                for(auto& ch : child){
                    ch.e = clip(bestE - ch.e);
                    ch.x = clip(isMaxNode() ? (x - ch.e) : (x + ch.e)); // 累積評価値更新
                }
                
                // 子ノードを相対評価順にソート
                /*std::sort(child.begin(), child.end(), [](const auto& c0, const auto& c1)->bool{
                 return c0.e < c1.e;
                 });*/
                
                // 子ノードの証明数、反証数関数を初期化
                for(auto& ch : child){
                    ch.vList.init(ch.x);
                    DERR << "child " << ch.x << " " << ch.vList << endl;
                    assert(vList.exam());
                }
                
                // 証明数、反証数関数を更新
                update(&child[0]);
            }else{
                
                terminal = true;
                vList.initTerminal(x);
                assert(vList.exam());
            }
        }
    }
    
    void update(const Node<board_t> *const pchild){
        
        if(isMaxNode()){
            DERR << "max ";
            // minimax値の更新
            if(pchild->vList.minimaxValue >= vList.minimaxValue){
                vList.minimaxValue = pchild->vList.minimaxValue;
            }else{
                vList.minimaxValue = -999999;
                for(const auto& ch : child){
                    vList.minimaxValue = max(vList.minimaxValue, ch.vList.minimaxValue);
                }
            }
            vList.atValue(vList.minimaxValue) = 0;
            // 証明数、反証数関数の更新
            // p
            for(int e = vList.minimaxValue + 1; e <= VALUE_INF; ++e){
                vList.atValue(e) = 999999;
                for(const auto& ch : child){
                    vList.atValue(e) = min(vList.atValue(e), pn(ch.vList, e));
                }
            }
            // d
            for(int e = -VALUE_INF; e < vList.minimaxValue; ++e){
                vList.atValue(e) = 0;
                for(const auto& ch : child){
                    vList.atValue(e) += dn(ch.vList, e);
                }
            }
        }else{
            DERR << "min ";
            // minimax値の更新
            if(pchild->vList.minimaxValue <= vList.minimaxValue){
                vList.minimaxValue = pchild->vList.minimaxValue;
            }else{
                vList.minimaxValue = 999999;
                for(const auto& ch : child){
                    vList.minimaxValue = min(vList.minimaxValue, ch.vList.minimaxValue);
                }
            }
            vList.atValue(vList.minimaxValue) = 0;
            // 証明数、反証数関数の更新
            // p
            for(int e = vList.minimaxValue + 1; e <= VALUE_INF; ++e){
                vList.atValue(e) = 0;
                for(const auto& ch : child){
                    vList.atValue(e) += pn(ch.vList, e);
                }
            }
            // d
            for(int e = -VALUE_INF; e < vList.minimaxValue; ++e){
                vList.atValue(e) = 999999;
                for(const auto& ch : child){
                    vList.atValue(e) = min(vList.atValue(e), dn(ch.vList, e));
                }
            }
        }
        
        DERR << x << " " << vList << endl;
        assert(vList.exam());
        
        ++count;
    }
};

template<class node_t>
node_t* selectMPN(node_t& node, double Vmin, double Vmax){
    if(node.isLeaf()){
        return &node;
    }
    
    int chosen = 0;
    int mininum = INT_MAX;
    
    if(node.isMaxNode()){

        for(std::size_t i = 0; i < node.child.size(); ++i){
            int tmp = pn(node.child[i].vList, Vmax);
            if(tmp < mininum){
                chosen = i;
                mininum = tmp;
            }
        }
        
    }else{
        
        for(std::size_t i = 0; i < node.child.size(); ++i){
            int tmp = dn(node.child[i].vList, Vmin);
            if(tmp < mininum){
                chosen = i;
                mininum = tmp;
            }
        }
        
    }
    
    return selectMPN(node.child[chosen], Vmin, Vmax);
}

template<class node_t>
Move getBestChildMove(node_t& nd){
    Move best = MOVE_RESIGN;
    int bestCount = -1;
    for(const auto& ch : nd.child){
        if(ch.count > bestCount){
            best = ch.move;
            bestCount = ch.count;
        }
    }
    return best;
}

template<class board_t>
Move scns(board_t& pos){
    
    gMoveCount = 0;
    gMaxDepth = 0;
    gClock.start();
    
    Node<board_t> rootNode(pos);
    gRootColor = pos.side_to_move();
    
    int Pmax = 1;
    int Dmax = 1;
    
    int iteration = 0;
    int Vmax = 0, Vmin = 0;
    for(; gClock.stop() < 9000; ++iteration){
        
        // Vmax, Vmin を Pmax, Dmax から計算
        Vmax = vmaxByDef(rootNode.vList, Pmax);
        Vmin = vminByDef(rootNode.vList, Dmax);
        
        //cerr << rootNode.vList << endl;
        //cerr << "Vmax = " << Vmax << " Vmin = " << Vmin << endl;
        
        // 次に訪問するノードを決定
        Node<board_t> *pnode = selectMPN(rootNode, Vmin, Vmax);
        
        // expand nodes
        pnode->expand();
        
        // update nodes
        while(pnode->parent != nullptr){
            const Node<board_t> *const pchild = pnode;
            pnode = pnode->parent;
            
            pnode->update(pchild);
        }
        
        //cerr << "iteration " << iteration << " nodes = " << gMoveCount << " depth = " << gMaxDepth << endl;
        //getchar();
    }
    
    cerr << "summary : iteration " << iteration << " nodes = " << gMoveCount << " depth = " << gMaxDepth;
    cerr << " v = (" << Vmin << ", " << Vmax << ")" << endl;
    
    return getBestChildMove(rootNode);
}

#endif // SHOGI_RAPUNZEL_SCNS_HPP_

