#include "../include/AST.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {
std::string makeIndent(int depth) {
	return std::string(depth * 2, ' ');
}

void appendLine(std::ostringstream& out, int depth, const std::string& text) {
	out << makeIndent(depth) << text << "\n";
}

void dumpNode(const std::shared_ptr<ASTNode>& node, std::ostringstream& out, int depth) {
	if (node == nullptr) {
		appendLine(out, depth, "<null>");
		return;
	}

	appendLine(
		out,
		depth,
		node->getValue() + " [line " + std::to_string(node->getLineNumber()) + "]"
	);

	if (auto prog = std::dynamic_pointer_cast<ProgNode>(node)) {
		appendLine(out, depth + 1, "classes:");
		for (const auto& cls : prog->getClasses()) {
			dumpNode(cls, out, depth + 2);
		}

		appendLine(out, depth + 1, "functions:");
		for (const auto& fn : prog->getFunctions()) {
			dumpNode(fn, out, depth + 2);
		}
		return;
	}

	if (auto cls = std::dynamic_pointer_cast<ClassDeclNode>(node)) {
		const auto parents = cls->getParents();
		if (!parents.empty()) {
			appendLine(out, depth + 1, "inherits:");
			for (const auto& p : parents) {
				appendLine(out, depth + 2, p);
			}
		}

		appendLine(out, depth + 1, "members:");
		for (const auto& member : cls->getMembers()) {
			dumpNode(member, out, depth + 2);
		}
		return;
	}

	if (auto fn = std::dynamic_pointer_cast<FuncDefNode>(node)) {
		appendLine(out, depth + 1, "params:");
		for (const auto& param : fn->getParams()) {
			dumpNode(param, out, depth + 2);
		}

		appendLine(out, depth + 1, "locals:");
		for (const auto& local : fn->getLocalVars()) {
			dumpNode(local, out, depth + 2);
		}

		appendLine(out, depth + 1, "body:");
		dumpNode(fn->getRight(), out, depth + 2);
		return;
	}

	if (auto block = std::dynamic_pointer_cast<BlockNode>(node)) {
		for (const auto& stmt : block->getStatements()) {
			dumpNode(stmt, out, depth + 1);
		}
		return;
	}

	if (auto call = std::dynamic_pointer_cast<FuncCallNode>(node)) {
		if (call->getLeft() != nullptr) {
			appendLine(out, depth + 1, "callee:");
			dumpNode(call->getLeft(), out, depth + 2);
		}

		appendLine(out, depth + 1, "args:");
		for (const auto& arg : call->getArgs()) {
			dumpNode(arg, out, depth + 2);
		}
		return;
	}

	if (auto member = std::dynamic_pointer_cast<DataMemberNode>(node)) {
		if (member->getLeft() != nullptr) {
			appendLine(out, depth + 1, "owner:");
			dumpNode(member->getLeft(), out, depth + 2);
		}

		const auto indices = member->getIndices();
		if (!indices.empty()) {
			appendLine(out, depth + 1, "indices:");
			for (const auto& idx : indices) {
				dumpNode(idx, out, depth + 2);
			}
		}
		return;
	}

	if (auto ifNode = std::dynamic_pointer_cast<IfStmtNode>(node)) {
		appendLine(out, depth + 1, "cond:");
		dumpNode(ifNode->getLeft(), out, depth + 2);
		appendLine(out, depth + 1, "then:");
		dumpNode(ifNode->getRight(), out, depth + 2);
		if (ifNode->getElseBlock() != nullptr) {
			appendLine(out, depth + 1, "else:");
			dumpNode(ifNode->getElseBlock(), out, depth + 2);
		}
		return;
	}

	if (node->getLeft() != nullptr) {
		appendLine(out, depth + 1, "left:");
		dumpNode(node->getLeft(), out, depth + 2);
	}

	if (node->getRight() != nullptr) {
		appendLine(out, depth + 1, "right:");
		dumpNode(node->getRight(), out, depth + 2);
	}
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

std::string nodeLabel(const std::shared_ptr<ASTNode>& node) {
	if (node == nullptr) {
		return "<null>";
	}
	return node->getValue() + " [line " + std::to_string(node->getLineNumber()) + "]";
}

void ensureDotNode(
	const std::shared_ptr<ASTNode>& node,
	std::unordered_map<const ASTNode*, std::string>& ids,
	std::vector<std::string>& nodes,
	int& counter
) {
	if (node == nullptr || ids.find(node.get()) != ids.end()) {
		return;
	}

	std::string id = "n" + std::to_string(counter++);
	ids[node.get()] = id;
	nodes.push_back("  " + id + " [label=\"" + escapeDotLabel(nodeLabel(node)) + "\"];");
}

void addEdge(
	const std::string& from,
	const std::string& to,
	const std::string& label,
	std::vector<std::string>& edges
) {
	if (label.empty()) {
		edges.push_back("  " + from + " -> " + to + ";");
	} else {
		edges.push_back("  " + from + " -> " + to + " [label=\"" + escapeDotLabel(label) + "\"];");
	}
}

void walkDot(
	const std::shared_ptr<ASTNode>& node,
	std::unordered_map<const ASTNode*, std::string>& ids,
	std::vector<std::string>& nodes,
	std::vector<std::string>& edges,
	int& counter
) {
	if (node == nullptr) {
		return;
	}

	ensureDotNode(node, ids, nodes, counter);
	const std::string fromId = ids[node.get()];

	if (auto prog = std::dynamic_pointer_cast<ProgNode>(node)) {
		for (const auto& cls : prog->getClasses()) {
			ensureDotNode(cls, ids, nodes, counter);
			addEdge(fromId, ids[cls.get()], "class", edges);
			walkDot(cls, ids, nodes, edges, counter);
		}
		for (const auto& fn : prog->getFunctions()) {
			ensureDotNode(fn, ids, nodes, counter);
			addEdge(fromId, ids[fn.get()], "function", edges);
			walkDot(fn, ids, nodes, edges, counter);
		}
		return;
	}

	if (auto cls = std::dynamic_pointer_cast<ClassDeclNode>(node)) {
		for (const auto& member : cls->getMembers()) {
			ensureDotNode(member, ids, nodes, counter);
			addEdge(fromId, ids[member.get()], "member", edges);
			walkDot(member, ids, nodes, edges, counter);
		}
		return;
	}

	if (auto fn = std::dynamic_pointer_cast<FuncDefNode>(node)) {
		for (const auto& param : fn->getParams()) {
			ensureDotNode(param, ids, nodes, counter);
			addEdge(fromId, ids[param.get()], "param", edges);
			walkDot(param, ids, nodes, edges, counter);
		}
		for (const auto& local : fn->getLocalVars()) {
			ensureDotNode(local, ids, nodes, counter);
			addEdge(fromId, ids[local.get()], "local", edges);
			walkDot(local, ids, nodes, edges, counter);
		}
		if (fn->getRight() != nullptr) {
			ensureDotNode(fn->getRight(), ids, nodes, counter);
			addEdge(fromId, ids[fn->getRight().get()], "body", edges);
			walkDot(fn->getRight(), ids, nodes, edges, counter);
		}
		return;
	}

	if (auto block = std::dynamic_pointer_cast<BlockNode>(node)) {
		for (const auto& stmt : block->getStatements()) {
			ensureDotNode(stmt, ids, nodes, counter);
			addEdge(fromId, ids[stmt.get()], "stmt", edges);
			walkDot(stmt, ids, nodes, edges, counter);
		}
		return;
	}

	if (auto call = std::dynamic_pointer_cast<FuncCallNode>(node)) {
		if (call->getLeft() != nullptr) {
			ensureDotNode(call->getLeft(), ids, nodes, counter);
			addEdge(fromId, ids[call->getLeft().get()], "callee", edges);
			walkDot(call->getLeft(), ids, nodes, edges, counter);
		}
		for (const auto& arg : call->getArgs()) {
			ensureDotNode(arg, ids, nodes, counter);
			addEdge(fromId, ids[arg.get()], "arg", edges);
			walkDot(arg, ids, nodes, edges, counter);
		}
		return;
	}

	if (auto member = std::dynamic_pointer_cast<DataMemberNode>(node)) {
		if (member->getLeft() != nullptr) {
			ensureDotNode(member->getLeft(), ids, nodes, counter);
			addEdge(fromId, ids[member->getLeft().get()], "owner", edges);
			walkDot(member->getLeft(), ids, nodes, edges, counter);
		}
		for (const auto& idx : member->getIndices()) {
			ensureDotNode(idx, ids, nodes, counter);
			addEdge(fromId, ids[idx.get()], "index", edges);
			walkDot(idx, ids, nodes, edges, counter);
		}
		return;
	}

	if (auto ifNode = std::dynamic_pointer_cast<IfStmtNode>(node)) {
		if (ifNode->getLeft() != nullptr) {
			ensureDotNode(ifNode->getLeft(), ids, nodes, counter);
			addEdge(fromId, ids[ifNode->getLeft().get()], "cond", edges);
			walkDot(ifNode->getLeft(), ids, nodes, edges, counter);
		}
		if (ifNode->getRight() != nullptr) {
			ensureDotNode(ifNode->getRight(), ids, nodes, counter);
			addEdge(fromId, ids[ifNode->getRight().get()], "then", edges);
			walkDot(ifNode->getRight(), ids, nodes, edges, counter);
		}
		if (ifNode->getElseBlock() != nullptr) {
			ensureDotNode(ifNode->getElseBlock(), ids, nodes, counter);
			addEdge(fromId, ids[ifNode->getElseBlock().get()], "else", edges);
			walkDot(ifNode->getElseBlock(), ids, nodes, edges, counter);
		}
		return;
	}

    if (auto whileNode = std::dynamic_pointer_cast<WhileStmtNode>(node)) {
        if (whileNode->getLeft() != nullptr) {
            ensureDotNode(whileNode->getLeft(), ids, nodes, counter);
            addEdge(fromId, ids[whileNode->getLeft().get()], "cond", edges);
            walkDot(whileNode->getLeft(), ids, nodes, edges, counter);
        }
        if (whileNode->getRight() != nullptr) {
            ensureDotNode(whileNode->getRight(), ids, nodes, counter);
            addEdge(fromId, ids[whileNode->getRight().get()], "body", edges);
            walkDot(whileNode->getRight(), ids, nodes, edges, counter);
        }
        return;
    }

    if (auto assignNode = std::dynamic_pointer_cast<AssignStmtNode>(node)) {
        if (assignNode->getLeft() != nullptr) {
            ensureDotNode(assignNode->getLeft(), ids, nodes, counter);
            addEdge(fromId, ids[assignNode->getLeft().get()], "target", edges);
            walkDot(assignNode->getLeft(), ids, nodes, edges, counter);
        }
        if (assignNode->getRight() != nullptr) {
            ensureDotNode(assignNode->getRight(), ids, nodes, counter);
            addEdge(fromId, ids[assignNode->getRight().get()], "value", edges);
            walkDot(assignNode->getRight(), ids, nodes, edges, counter);
        }
        return;
    }

    if (auto ioNode = std::dynamic_pointer_cast<IOStmtNode>(node)) {
        if (ioNode->getLeft() != nullptr) {
            ensureDotNode(ioNode->getLeft(), ids, nodes, counter);
            addEdge(fromId, ids[ioNode->getLeft().get()], "target", edges);
            walkDot(ioNode->getLeft(), ids, nodes, edges, counter);
        }
        return;
    }

	if (node->getLeft() != nullptr) {
		ensureDotNode(node->getLeft(), ids, nodes, counter);
		addEdge(fromId, ids[node->getLeft().get()], "left", edges);
		walkDot(node->getLeft(), ids, nodes, edges, counter);
	}

	if (node->getRight() != nullptr) {
		ensureDotNode(node->getRight(), ids, nodes, counter);
		addEdge(fromId, ids[node->getRight().get()], "right", edges);
		walkDot(node->getRight(), ids, nodes, edges, counter);
	}
}
}

std::string ASTPrinter::toString(const std::shared_ptr<ASTNode>& root) {
	std::ostringstream out;
	dumpNode(root, out, 0);
	return out.str();
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
	std::vector<std::string> dotNodes;
	std::vector<std::string> dotEdges;
	std::unordered_map<const ASTNode*, std::string> ids;
	int counter = 0;

	if (root != nullptr) {
		walkDot(root, ids, dotNodes, dotEdges, counter);
	}

	std::ostringstream out;
	out << "digraph AST {\n";
	out << "  rankdir=TB;\n";
	out << "  graph [fontname=\"Consolas\"];\n";
	out << "  node [shape=box, style=rounded, fontname=\"Consolas\"];\n";
	out << "  edge [fontname=\"Consolas\"];\n";

	for (const auto& line : dotNodes) {
		out << line << "\n";
	}
	for (const auto& line : dotEdges) {
		out << line << "\n";
	}
	out << "}\n";

	return out.str();
}

bool ASTPrinter::writeDotToFile(const std::shared_ptr<ASTNode>& root, const std::string& filePath) {
	std::ofstream file(filePath, std::ios::out | std::ios::trunc);
	if (!file.is_open()) {
		return false;
	}

	file << ASTPrinter::toDot(root);
	return true;
}
