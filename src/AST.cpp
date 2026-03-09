#include "../include/AST.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

void IdNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IntLitNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FloatLitNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void TypeNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BinaryOpNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void UnaryOpNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FuncCallNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void DataMemberNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void AssignStmtNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IfStmtNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void WhileStmtNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IOStmtNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ReturnStmtNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BlockNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VarDeclNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void FuncDefNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ClassDeclNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ProgNode::accept(ASTVisitor& visitor) { visitor.visit(*this); }

namespace {
std::string makeIndent(int depth) {
    return std::string(depth * 2, ' ');
}

std::string escapeDotLabel(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text) {
        if (c == '\\' || c == '"' || c == '<' || c == '>') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

std::string nodeLabel(ASTNode& node) {
    return node.getValue() + " [line " + std::to_string(node.getLineNumber()) + "]";
}

// ============================================================================
// TEXT VISITOR
// ============================================================================
class TextASTVisitor : public ASTVisitor {
    public:
        std::string str() const { return _out.str(); }

        void visit(IdNode& node) override { visitLeaf(node); }
        void visit(IntLitNode& node) override { visitLeaf(node); }
        void visit(FloatLitNode& node) override { visitLeaf(node); }
        void visit(TypeNode& node) override { visitLeaf(node); }
        void visit(VarDeclNode& node) override { visitLeaf(node); }

        void visit(BinaryOpNode& node) override { visitBinaryLike(node); }
        void visit(UnaryOpNode& node) override { visitBinaryLike(node); }
        void visit(AssignStmtNode& node) override { visitBinaryLike(node); }
        void visit(WhileStmtNode& node) override { visitBinaryLike(node, "cond", "body"); }
        void visit(IOStmtNode& node) override { visitBinaryLike(node); }
        void visit(ReturnStmtNode& node) override { visitBinaryLike(node); }

        void visit(IfStmtNode& node) override {
            writeNode(node);
            section("cond");
            visitChild(node.getLeft(), 0);
            section("then");
            visitChild(node.getRight(), 0);
            if (node.getElseBlock() != nullptr) {
                section("else");
                visitChild(node.getElseBlock(), 0);
            }
            _depth--;
        }

        void visit(FuncCallNode& node) override {
            writeNode(node);
            if (node.getLeft() != nullptr) {
                section("callee");
                visitChild(node.getLeft(), 0);
            }
            section("args");
            for (const auto& arg : node.getArgs()) {
                visitChild(arg, 1);
            }
            _depth--;
        }

        void visit(DataMemberNode& node) override {
            writeNode(node);
            if (node.getLeft() != nullptr) {
                section("owner");
                visitChild(node.getLeft(), 0);
            }
            if (!node.getIndices().empty()) {
                section("indices");
                for (const auto& idx : node.getIndices()) {
                    visitChild(idx, 1);
                }
            }
            _depth--;
        }

        void visit(BlockNode& node) override {
            writeNode(node);
            for (const auto& stmt : node.getStatements()) {
                visitChild(stmt, 0);
            }
            _depth--;
        }

        void visit(FuncDefNode& node) override {
            writeNode(node);
            section("params");
            for (const auto& param : node.getParams()) {
                visitChild(param, 1);
            }
            section("locals");
            for (const auto& local : node.getLocalVars()) {
                visitChild(local, 1);
            }
            section("body");
            visitChild(node.getRight(), 0);
            _depth--;
        }

        void visit(ClassDeclNode& node) override {
            writeNode(node);
            if (!node.getParents().empty()) {
                section("inherits");
                for (const auto& parent : node.getParents()) {
                    line(parent, _depth + 1);
                }
            }
            section("members");
            for (const auto& member : node.getMembers()) {
                visitChild(member, 1);
            }
            _depth--;
        }

        void visit(ProgNode& node) override {
            writeNode(node);
            section("classes");
            for (const auto& cls : node.getClasses()) {
                visitChild(cls, 1);
            }
            section("functions");
            for (const auto& fn : node.getFunctions()) {
                visitChild(fn, 1);
            }
            _depth--;
        }

    private:
        std::ostringstream _out;
        int _depth = 0;

        void line(const std::string& text, int depth) {
            _out << makeIndent(depth) << text << "\n";
        }

        void writeNode(ASTNode& node) {
            line(nodeLabel(node), _depth);
            _depth++;
        }

        void section(const std::string& label) {
            line(label + ":", _depth);
        }

        void visitChild(const std::shared_ptr<ASTNode>& node, int extraIndent) {
            if (node == nullptr) {
                line("<null>", _depth + extraIndent);
                return;
            }
            if (extraIndent == 1) {
                _depth++;
                node->accept(*this);
                _depth--;
            } else {
                node->accept(*this);
            }
        }

        void visitLeaf(ASTNode& node) {
            line(nodeLabel(node), _depth);
        }

        void visitBinaryLike(ASTNode& node, const std::string& leftLabel = "left", const std::string& rightLabel = "right") {
            writeNode(node);
            if (node.getLeft() != nullptr) {
                section(leftLabel);
                visitChild(node.getLeft(), 0);
            }
            if (node.getRight() != nullptr) {
                section(rightLabel);
                visitChild(node.getRight(), 0);
            }
            _depth--;
        }
};

// ============================================================================
// UML DOT VISITOR
// ============================================================================
class DotASTVisitor : public ASTVisitor {
    public:
        std::string dot() const {
            std::ostringstream out;
            out << "digraph AST {\n";
            out << "  rankdir=TB;\n";
            out << "  graph [fontname=\"Consolas\", splines=polyline];\n";
            out << "  node [fontname=\"Consolas\"];\n";
            out << "  edge [fontname=\"Consolas\", fontsize=10];\n\n";
            
            for (const auto& n : _nodeLines) out << n << "\n";
            out << "\n";
            for (const auto& e : _edgeLines) out << e << "\n";
            
            // UML Legend: multi-column reference table
            out << "\n  subgraph cluster_legend {\n";
            out << "    label = \"AST Legend\";\n";
            out << "    fontsize = 12;\n";
            out << "    style = dashed;\n";
            out << "    node [shape=plaintext];\n";
            out << "    legend_table [label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\">\n";
            out << "      <TR><TD BGCOLOR=\"#DDEBF7\"><B>Node Categories</B></TD><TD BGCOLOR=\"#E2F0D9\"><B>Edge Labels</B></TD><TD BGCOLOR=\"#FCE4D6\"><B>Arrow/Style</B></TD></TR>\n";
            out << "      <TR><TD BGCOLOR=\"#E8E8E8\">Program: folder</TD><TD>class, function</TD><TD>vee / solid</TD></TR>\n";
            out << "      <TR><TD BGCOLOR=\"#D9EAD3\">Class: record</TD><TD>member, inherits</TD><TD>odiamond / empty</TD></TR>\n";
            out << "      <TR><TD BGCOLOR=\"#CFE2F3\">Function: Mrecord</TD><TD>param, local, body</TD><TD>dot / solid</TD></TR>\n";
            out << "      <TR><TD BGCOLOR=\"#FCE5CD\">Variable/Member: note</TD><TD>owner, index</TD><TD>dot / dashed</TD></TR>\n";
            out << "      <TR><TD BGCOLOR=\"#FFF2CC\">Control Flow: diamond</TD><TD>cond, then, else</TD><TD>vee / dashed</TD></TR>\n";
            out << "      <TR><TD BGCOLOR=\"#EAD1DC\">Statement/Call: box</TD><TD>target, value, callee, arg</TD><TD>vee / solid,dashed</TD></TR>\n";
            out << "      <TR><TD BGCOLOR=\"#F3F3F3\">Expression/Leaf: ellipse</TD><TD>left, right</TD><TD>vee / solid</TD></TR>\n";
            out << "    </TABLE>>];\n";
            out << "  }\n";

            out << "}\n";
            return out.str();
        }

        // --- Leaves & Expressions ---
        void visit(IdNode& node) override { declareNode(node, "ellipse", "#F3F3F3"); }
        void visit(IntLitNode& node) override { declareNode(node, "ellipse", "#F3F3F3"); }
        void visit(FloatLitNode& node) override { declareNode(node, "ellipse", "#F3F3F3"); }
        void visit(TypeNode& node) override { declareNode(node, "ellipse", "#F3F3F3"); }
        void visit(BinaryOpNode& node) override { 
            declareNode(node, "ellipse", "#F3F3F3");
            visitBinaryLike(node); 
        }
        void visit(UnaryOpNode& node) override { 
            declareNode(node, "ellipse", "#F3F3F3");
            visitBinaryLike(node); 
        }

        // --- Variables & Members ---
        void visit(VarDeclNode& node) override { declareNode(node, "note", "#FCE5CD"); }
        void visit(DataMemberNode& node) override {
            declareNode(node, "note", "#FCE5CD");
            visitChild(node, node.getLeft(), "owner", "dot", "solid");
            for (const auto& idx : node.getIndices()) visitChild(node, idx, "index", "vee", "dashed");
        }

        // --- Statements ---
        void visit(AssignStmtNode& node) override { 
            declareNode(node, "box", "#EAD1DC");
            visitBinaryLike(node, "target", "value"); 
        }
        void visit(IOStmtNode& node) override { 
            declareNode(node, "box", "#EAD1DC");
            visitChild(node, node.getLeft(), "target"); 
        }
        void visit(ReturnStmtNode& node) override { 
            declareNode(node, "box", "#EAD1DC");
            visitChild(node, node.getLeft(), "value"); 
        }
        void visit(BlockNode& node) override {
            declareNode(node, "box", "#EAD1DC");
            for (const auto& stmt : node.getStatements()) visitChild(node, stmt, "stmt");
        }
        void visit(FuncCallNode& node) override {
            declareNode(node, "box", "#EAD1DC");
            visitChild(node, node.getLeft(), "callee", "dot");
            for (const auto& arg : node.getArgs()) visitChild(node, arg, "arg", "vee", "dashed");
        }

        // --- Control Flow ---
        void visit(WhileStmtNode& node) override { 
            declareNode(node, "diamond", "#FFF2CC");
            visitBinaryLike(node, "cond", "body"); 
        }
        void visit(IfStmtNode& node) override {
            declareNode(node, "diamond", "#FFF2CC");
            visitChild(node, node.getLeft(), "cond", "vee", "dashed");
            visitChild(node, node.getRight(), "then");
            visitChild(node, node.getElseBlock(), "else");
        }

        // --- Core UML Structures ---
        void visit(FuncDefNode& node) override {
            declareNode(node, "Mrecord", "#CFE2F3"); // Mrecord gives rounded corners to records
            for (const auto& param : node.getParams()) visitChild(node, param, "param", "dot");
            for (const auto& local : node.getLocalVars()) visitChild(node, local, "local", "dot");
            visitChild(node, node.getRight(), "body");
        }

        void visit(ClassDeclNode& node) override {
            declareNode(node, "record", "#D9EAD3"); 
            
            // UML Inheritance uses an empty arrowhead
            for (const auto& parentName : node.getParents()) {
                std::string parentDummyId = "n" + std::to_string(_counter++);
                _nodeLines.push_back("  " + parentDummyId + " [label=\"" + escapeDotLabel(parentName) + "\", shape=record, style=dashed];");
                
                std::string myId = getNodeId(&node);
                _edgeLines.push_back("  " + myId + " -> " + parentDummyId + " [label=\"inherits\", arrowhead=\"empty\", style=\"solid\"];");
            }

            for (const auto& member : node.getMembers()) {
                visitChild(node, member, "member", "odiamond"); // odiamond represents aggregation/composition
            }
        }

        void visit(ProgNode& node) override {
            declareNode(node, "folder", "#E8E8E8");
            for (const auto& cls : node.getClasses()) visitChild(node, cls, "class");
            for (const auto& fn : node.getFunctions()) visitChild(node, fn, "function");
        }

    private:
        std::unordered_map<const ASTNode*, std::string> _ids;
        std::unordered_set<const ASTNode*> _declared;
        std::vector<std::string> _nodeLines;
        std::vector<std::string> _edgeLines;
        int _counter = 0;

        std::string getNodeId(const ASTNode* ptr) {
            auto it = _ids.find(ptr);
            if (it != _ids.end()) return it->second;
            std::string id = "n" + std::to_string(_counter++);
            _ids[ptr] = id;
            return id;
        }

        void declareNode(ASTNode& node, const std::string& shape, const std::string& fillcolor) {
            const ASTNode* ptr = &node;
            if (_declared.count(ptr)) return; // Already customized
            _declared.insert(ptr);
            
            std::string id = getNodeId(ptr);
            _nodeLines.push_back("  " + id + " [label=\"" + escapeDotLabel(nodeLabel(node)) + "\", shape=" + shape + ", style=filled, fillcolor=\"" + fillcolor + "\"];");
        }

        void addEdge(ASTNode& parent, ASTNode& child, const std::string& label, const std::string& arrowhead, const std::string& style) {
            std::string from = getNodeId(&parent);
            std::string to = getNodeId(&child);
            std::string safeLabel = label.empty() ? "relation" : label;
            _edgeLines.push_back("  " + from + " -> " + to + " [label=\"" + escapeDotLabel(safeLabel) + "\", arrowhead=\"" + arrowhead + "\", style=\"" + style + "\"];");
        }

        void visitChild(ASTNode& parent, const std::shared_ptr<ASTNode>& child, const std::string& label, const std::string& arrowhead = "vee", const std::string& style = "solid") {
            if (child == nullptr) return;
            addEdge(parent, *child, label, arrowhead, style);
            child->accept(*this);
        }

        void visitBinaryLike(ASTNode& node, const std::string& leftLabel = "left", const std::string& rightLabel = "right") {
            visitChild(node, node.getLeft(), leftLabel);
            visitChild(node, node.getRight(), rightLabel);
        }
};
}

std::string ASTPrinter::toString(const std::shared_ptr<ASTNode>& root) {
    TextASTVisitor visitor;
    if (root == nullptr) return "<null>\n";
    root->accept(visitor);
    return visitor.str();
}

bool ASTPrinter::writeToFile(const std::shared_ptr<ASTNode>& root, const std::string& filePath) {
    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    file << ASTPrinter::toString(root);
    return true;
}

std::string ASTPrinter::toDot(const std::shared_ptr<ASTNode>& root) {
    DotASTVisitor visitor;
    if (root != nullptr) root->accept(visitor);
    return visitor.dot();
}

bool ASTPrinter::writeDotToFile(const std::shared_ptr<ASTNode>& root, const std::string& filePath) {
    std::ofstream file(filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    file << ASTPrinter::toDot(root);
    return true;
}