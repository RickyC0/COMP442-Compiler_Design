#include "../include/AST.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

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
		if (c == '\\' || c == '"') {
			escaped.push_back('\\');
		}
		escaped.push_back(c);
	}
	return escaped;
}

std::string nodeLabel(ASTNode& node) {
	return node.getValue() + " [line " + std::to_string(node.getLineNumber()) + "]";
}

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

class DotASTVisitor : public ASTVisitor {
	public:
		std::string dot() const {
			std::ostringstream out;
			out << "digraph AST {\n";
			out << "  rankdir=TB;\n";
			out << "  graph [fontname=\"Consolas\"];\n";
			out << "  node [shape=box, style=rounded, fontname=\"Consolas\"];\n";
			out << "  edge [fontname=\"Consolas\"];\n";
			for (const auto& n : _nodeLines) {
				out << n << "\n";
			}
			for (const auto& e : _edgeLines) {
				out << e << "\n";
			}
			out << "}\n";
			return out.str();
		}

		void visit(IdNode& node) override { ensureNode(node); }
		void visit(IntLitNode& node) override { ensureNode(node); }
		void visit(FloatLitNode& node) override { ensureNode(node); }
		void visit(TypeNode& node) override { ensureNode(node); }
		void visit(VarDeclNode& node) override { ensureNode(node); }

		void visit(BinaryOpNode& node) override { visitBinaryLike(node); }
		void visit(UnaryOpNode& node) override { visitBinaryLike(node); }
		void visit(AssignStmtNode& node) override { visitBinaryLike(node, "target", "value"); }
		void visit(WhileStmtNode& node) override { visitBinaryLike(node, "cond", "body"); }
		void visit(IOStmtNode& node) override { visitBinaryLike(node, "target"); }
		void visit(ReturnStmtNode& node) override { visitBinaryLike(node, "value"); }

		void visit(IfStmtNode& node) override {
			ensureNode(node);
			visitChild(node, node.getLeft(), "cond");
			visitChild(node, node.getRight(), "then");
			visitChild(node, node.getElseBlock(), "else");
		}

		void visit(FuncCallNode& node) override {
			ensureNode(node);
			visitChild(node, node.getLeft(), "callee");
			for (const auto& arg : node.getArgs()) {
				visitChild(node, arg, "arg");
			}
		}

		void visit(DataMemberNode& node) override {
			ensureNode(node);
			visitChild(node, node.getLeft(), "owner");
			for (const auto& idx : node.getIndices()) {
				visitChild(node, idx, "index");
			}
		}

		void visit(BlockNode& node) override {
			ensureNode(node);
			for (const auto& stmt : node.getStatements()) {
				visitChild(node, stmt, "stmt");
			}
		}

		void visit(FuncDefNode& node) override {
			ensureNode(node);
			for (const auto& param : node.getParams()) {
				visitChild(node, param, "param");
			}
			for (const auto& local : node.getLocalVars()) {
				visitChild(node, local, "local");
			}
			visitChild(node, node.getRight(), "body");
		}

		void visit(ClassDeclNode& node) override {
            ensureNode(node);
            
            // Create dashed dummy nodes to show inheritance in the graph
            for (const auto& parentName : node.getParents()) {
                std::string parentDummyId = "n" + std::to_string(_counter++);
                _nodeLines.push_back("  " + parentDummyId + " [label=\"Parent: " + escapeDotLabel(parentName) + "\", style=dashed];");
                _edgeLines.push_back("  " + ensureNode(node) + " -> " + parentDummyId + " [label=\"inherits\"];");
            }

            // Process actual class members
            for (const auto& member : node.getMembers()) {
                visitChild(node, member, "member");
            }
        }

		void visit(ProgNode& node) override {
			ensureNode(node);
			for (const auto& cls : node.getClasses()) {
				visitChild(node, cls, "class");
			}
			for (const auto& fn : node.getFunctions()) {
				visitChild(node, fn, "function");
			}
		}

	private:
		std::unordered_map<const ASTNode*, std::string> _ids;
		std::vector<std::string> _nodeLines;
		std::vector<std::string> _edgeLines;
		int _counter = 0;

		std::string ensureNode(ASTNode& node) {
			const ASTNode* ptr = &node;
			auto it = _ids.find(ptr);
			if (it != _ids.end()) {
				return it->second;
			}

			std::string id = "n" + std::to_string(_counter++);
			_ids[ptr] = id;
			_nodeLines.push_back("  " + id + " [label=\"" + escapeDotLabel(nodeLabel(node)) + "\"];");
			return id;
		}

		void addEdge(ASTNode& parent, ASTNode& child, const std::string& label) {
			const std::string from = ensureNode(parent);
			const std::string to = ensureNode(child);
			_edgeLines.push_back("  " + from + " -> " + to + " [label=\"" + escapeDotLabel(label) + "\"];");
		}

		void visitChild(ASTNode& parent, const std::shared_ptr<ASTNode>& child, const std::string& label) {
			if (child == nullptr) {
				return;
			}
			addEdge(parent, *child, label);
			child->accept(*this);
		}

		void visitBinaryLike(ASTNode& node, const std::string& leftLabel = "left", const std::string& rightLabel = "right") {
			ensureNode(node);
			visitChild(node, node.getLeft(), leftLabel);
			visitChild(node, node.getRight(), rightLabel);
		}
};
}

std::string ASTPrinter::toString(const std::shared_ptr<ASTNode>& root) {
	TextASTVisitor visitor;
	if (root == nullptr) {
		return "<null>\n";
	}
	root->accept(visitor);
	return visitor.str();
}

bool ASTPrinter::writeToFile(const std::shared_ptr<ASTNode>& root, const std::string& filePath) {
	std::ofstream file(filePath, std::ios::out | std::ios::trunc);
	if (!file.is_open()) {
		return false;
	}

	file << ASTPrinter::toString(root);
	return true;
}

std::string ASTPrinter::toDot(const std::shared_ptr<ASTNode>& root) {
	DotASTVisitor visitor;
	if (root != nullptr) {
		root->accept(visitor);
	}
	return visitor.dot();
}

bool ASTPrinter::writeDotToFile(const std::shared_ptr<ASTNode>& root, const std::string& filePath) {
	std::ofstream file(filePath, std::ios::out | std::ios::trunc);
	if (!file.is_open()) {
		return false;
	}

	file << ASTPrinter::toDot(root);
	return true;
}
