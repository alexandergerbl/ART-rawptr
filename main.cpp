#include<iostream>
#include<cstdint>
#include<algorithm>
#include<array>
#include<vector>
#include<memory>
#include<functional>
#include<string>
#include<utility>

using namespace std;

typedef uint8_t Key [8];
typedef string Value;

const uint8_t EMPTY(50);

enum class Nodetype : uint8_t { SVLeaf = 1, Node4 = 4, Node16 = 16, Node48 = 48, Node256 = 5 };

struct BaseNode
{
	Nodetype type;
	BaseNode(Nodetype type) : type(type) {};
	
	/// Tested
	/* 
	 * @brief: returns whether a node is a leaf or not
	 * @params:	node 		node to check for being a leaf
	 **/
	bool inline isLeaf()
	{
		return (this->type == Nodetype::SVLeaf);
	}
};
///	SVLeaf - Single-Value Leaf
struct SVLeaf : BaseNode
{
	Key key;
	Value value;
	SVLeaf(Key k, Value v) : BaseNode(Nodetype::SVLeaf)
	{
		this->value = v;
		copy(k, k+8, key);
	};
	/// Tested
	/* check if leaf fully matches the key
	 * @return:	returns whether the key belongs to the leaf
	 */
	bool leafMatches(const Key& key, uint8_t depth)
	{
		return equal(this->key+depth, this->key+8, key+depth);	
	}
};

struct InnerNode : BaseNode
{
	int numChildren;
	uint8_t prefixLen;
	uint8_t prefix[8];
	InnerNode(Nodetype type) : BaseNode(type), numChildren(0), prefixLen(0) {};

	/// Tested
	/* @brief: returns number of bytes where key and node's prefix are equal, starting at depth
	 *
	 */
	int checkPrefix(const Key& key, uint8_t depth)
	{
		auto count = 0;
		for(auto i = depth; (key[i] == this->prefix[i-depth]) && (i < depth + this->prefixLen); i++)
			count++;
		return count;
	}
	/// Tested
	/// Returns if a node is full 
	bool isFull()
	{
		return (this->type == Nodetype::Node256) ? (this->numChildren == 256) : (this->numChildren == (int)this->type);
	}
};

struct Node4 : InnerNode
{
	array<uint8_t, 4> keys;
	array<BaseNode*, 4> child;
	Node4() : InnerNode(Nodetype::Node4)
	{
		fill(child.begin(), child.end(), nullptr);
	}
};

struct Node16 : InnerNode
{
	array<uint8_t, 16> keys;
	array<BaseNode*, 16> child;
	Node16() : InnerNode(Nodetype::Node16) 
	{
		fill(child.begin(), child.end(), nullptr);
		fill(keys.begin(), keys.end(), EMPTY);	
	}
};

struct Node48 : InnerNode
{
	array<uint8_t, 256> index;
	array<BaseNode*, 48> child;
	Node48() : InnerNode(Nodetype::Node48)
	{
		fill(child.begin(), child.end(), nullptr);
		fill(index.begin(), index.end(), EMPTY);
	}

};

struct Node256 : InnerNode
{
	array<BaseNode*, 256> child;
	Node256() : InnerNode(Nodetype::Node256)
	{
		fill(child.begin(), child.end(), nullptr);
	}
};

BaseNode** findChild(BaseNode* node, uint8_t byte)
{
	if(node->type == Nodetype::Node4)
	{
		Node4* tmp = static_cast<Node4*>(node);
		auto index = find(tmp->keys.begin(), tmp->keys.end(), byte);
		if(index != tmp->keys.end())
			return &tmp->child[(index-tmp->keys.begin())];
		return nullptr;
	}
	if(node->type == Nodetype::Node16)
	{
		Node16* tmp = static_cast<Node16*>(node);
		auto index = find(tmp->keys.begin(), tmp->keys.end(), byte);
		if(index != tmp->keys.end())
			return &tmp->child[(index-tmp->keys.begin())];
		return nullptr;
	}
	if(node->type == Nodetype::Node48)
	{
		Node48* tmp = static_cast<Node48*>(node);
		if(tmp->index[byte] != EMPTY)
			return &tmp->child[(tmp->index[byte])];
		return nullptr;
	}
	Node256* tmp = static_cast<Node256*>(node);
	return &tmp->child[(int)byte];
}
/// Tested - take care as script tells parent musn't be a SVLeaf and parent musn't be full
/// Appends a new child to an inner node NUMCHILDREN++
void addChild(BaseNode*& parent, uint8_t byte, BaseNode* child)
{
	//Case for total beginning root of ART root = nullptr! ! !
	if(parent == nullptr)
		parent = child;
	if(parent->type == Nodetype::Node4)
	{
		Node4* tmp_parent = static_cast<Node4*>(parent);
		tmp_parent->numChildren++;
		tmp_parent->keys[tmp_parent->numChildren-1] = byte;		
		sort(tmp_parent->keys.begin(), tmp_parent->keys.begin() + (tmp_parent->numChildren));//-1
		auto index_itr = find(tmp_parent->keys.begin(), tmp_parent->keys.end(), byte);
		auto index = index_itr - tmp_parent->keys.begin();
		if(index < tmp_parent->numChildren)
			move_backward(tmp_parent->child.begin() + index, tmp_parent->child.begin()+tmp_parent->numChildren-1, tmp_parent->child.begin() + tmp_parent->numChildren);
		tmp_parent->child[index] = child; 
		return;
	}
	if(parent->type == Nodetype::Node16)
	{
		Node16* tmp_parent = static_cast<Node16*>(parent);
		tmp_parent->numChildren++;
		tmp_parent->keys[tmp_parent->numChildren-1] = byte;
		sort(tmp_parent->keys.begin(), tmp_parent->keys.begin() + (tmp_parent->numChildren));
		auto index_itr = find(tmp_parent->keys.begin(), tmp_parent->keys.end(), byte);
		auto index = index_itr - tmp_parent->keys.begin();
		if(index < tmp_parent->numChildren)
			move_backward(tmp_parent->child.begin() + index, tmp_parent->child.begin()+tmp_parent->numChildren-1, tmp_parent->child.begin() + tmp_parent->numChildren);
		tmp_parent->child[index] = child;
		return;
	}
	if(parent->type == Nodetype::Node48)
	{
		Node48* tmp_parent = static_cast<Node48*>(parent);
		tmp_parent->child[tmp_parent->numChildren] = child;
		tmp_parent->index[byte] = tmp_parent->numChildren;		
		tmp_parent->numChildren++;	
		return;	
	}
	Node256* tmp_parent = static_cast<Node256*>(parent);
	tmp_parent->child[byte] = child;
	tmp_parent->numChildren++;
}
/// grows to one step bigger node e.g. Node4 -> Node16
void grow(InnerNode*& node)
{
	if(node->type == Nodetype::Node4)
	{
		Node4* tmp_node = static_cast<Node4*>(node);
		Node16* newNode = new Node16();
		//copy header
		newNode->numChildren = tmp_node->numChildren;
		newNode->prefixLen = tmp_node->prefixLen;
		copy(tmp_node->prefix, tmp_node->prefix + tmp_node->prefixLen, newNode->prefix);
		//move content
		move(tmp_node->keys.begin(), tmp_node->keys.end(), newNode->keys.begin());
		move(tmp_node->child.begin(), tmp_node->child.end(), newNode->child.begin());
		node = newNode;
		return; 
	}
	if(node->type == Nodetype::Node16)
	{
		InnerNode* newNode = new Node48();
		Node16* tmp_node = static_cast<Node16*>(node);
		//copy header
		newNode->prefixLen = tmp_node->prefixLen;
		BaseNode* base_newNode = static_cast<BaseNode*>(newNode);
		copy(tmp_node->prefix, tmp_node->prefix + tmp_node->prefixLen, newNode->prefix);
		
		auto count = 0;
		for_each(tmp_node->keys.begin(), tmp_node->keys.end(), [&count, &base_newNode, &tmp_node](uint8_t key)mutable{ addChild(base_newNode, key, tmp_node->child[count]); count++; });
		
		node = static_cast<InnerNode*>(newNode);
		return;
	}
	if(node->type == Nodetype::Node48)
	{
		Node256* newNode = new Node256();
		Node48* tmp_node = static_cast<Node48*>(node);
		//copy header
		newNode->prefixLen = tmp_node->prefixLen;
		copy(tmp_node->prefix, tmp_node->prefix + tmp_node->prefixLen, newNode->prefix);
		newNode->numChildren = tmp_node->numChildren;

		for(unsigned int i = 0; i < tmp_node->index.size(); i++)
		{
			if(tmp_node->index[i] != EMPTY)
				newNode->child[i] = tmp_node->child[tmp_node->index[i]];
		}	
		node = newNode;
		return;
	}
}
/// Tested
/// searches for a key starting from node and returns key corresponding leaf
BaseNode* search(BaseNode*& node, const Key& key, uint8_t depth)
{
	if(node == nullptr)
		return nullptr;
	if(node->isLeaf())
	{
		SVLeaf* tmp_node = static_cast<SVLeaf*>(node);
		if(tmp_node->leafMatches(key, depth))
			return tmp_node;
		return nullptr;
	}
	InnerNode* tmp_node = static_cast<InnerNode*>(node);
	if(tmp_node->checkPrefix(key, depth) != tmp_node->prefixLen)
		return nullptr;
	depth += tmp_node->prefixLen;
	auto next = findChild(tmp_node, key[depth]);
	return (search(*next, key, depth+1));
} 

void insert(BaseNode*& node, const Key& key, SVLeaf* leaf, int depth)
{
	if(node == nullptr)
	{
		node = leaf;
		return;
	}
	if(node->isLeaf())
	{
		Node4* newNode = new Node4();
		SVLeaf* node_leaf = static_cast<SVLeaf*>(node);

		Key key2;
		copy(node_leaf->key, node_leaf->key+8, key2);
	
		SVLeaf* newLeaf = new SVLeaf(key2, node_leaf->value);

		auto i = depth;
		for(;key[i] == key2[i];i++)
			newNode->prefix[i-depth] = key[i];
		newNode->prefixLen = i-depth;

		depth += newNode->prefixLen;
		BaseNode* newNode_BN = static_cast<BaseNode*>(newNode);
		addChild(newNode_BN, key[depth], leaf);
		addChild(newNode_BN, key2[depth], newLeaf);
		node = newNode_BN;
		return;
	}
	InnerNode* inner_node = static_cast<InnerNode*>(node);
	auto p = inner_node->checkPrefix(key, depth);
	if(p != inner_node->prefixLen)
	{
		BaseNode* newNode = new Node4();
		//create newLeaf depending on node->type
		BaseNode* newLeaf;
		swap(newLeaf, node);

		addChild(newNode, key[depth+p], leaf);
		addChild(newNode, inner_node->prefix[p], newLeaf);
		swap(newLeaf, node);
		InnerNode* inner_newNode = static_cast<InnerNode*>(newNode);
		inner_newNode->prefixLen = p;
		//save partial compressed key in parent
		copy(inner_node->prefix, inner_node->prefix + inner_node->prefixLen, inner_newNode->prefix);
		inner_node->prefixLen = inner_node->prefixLen-(p+1);
		//old parent new child doenst have right prefixLen ! ! ! 
		copy_backward(inner_node->prefix, inner_node->prefix + inner_node->prefixLen, inner_node->prefix + (p+1));
		node = newNode;
		return;
	}
	depth+=inner_node->prefixLen;
	//next must be by reference ! ! !
	BaseNode** next = findChild(inner_node, key[depth]);
	if((*next) != nullptr)
	{
	    insert(*next, key, leaf, depth+1);
	}
	else
	{
	    if(inner_node->isFull())
	      grow(inner_node);
	    addChild(node, key[depth], leaf);
	}
}
int main()
{
	BaseNode* root = nullptr;
	cout <<"root = " << root << "\n"<< endl;


	Key k { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
	Key k2 { 0x00, 0x01, 0x03, 0x03, 0x04, 0x05, 0x06, 0x07 };
	Key k3 { 0x00, 0x02, 0x02, 0x04, 0x04, 0x05, 0x06, 0x07 };
	Key k4 { 0x00, 0x01, 0x02, 0x03, 0x04, 0x06, 0x06, 0x07 };


	SVLeaf* leaf = new SVLeaf(k, "leaf");
	cout <<"leaf = " << leaf << endl;
	
	SVLeaf* leaf2 = new SVLeaf(k2, "leaf2");
	cout <<"leaf2 = " << leaf2 << endl;
	SVLeaf* leaf3 = new SVLeaf(k3, "leaf3");
	cout <<"leaf3 = " << leaf3 << endl;
	SVLeaf* leaf4 = new SVLeaf(k4, "leaf4");
	cout <<"leaf4 = " << leaf4 << "\n"<< endl;

/*	addChild(root, 0x01, leaf);
	addChild(root, 0x02, leaf2);
	addChild(root, 0x03, leaf3);

	SVLeaf** blatt = reinterpret_cast<SVLeaf**>(findChild(root, 0x01));

	cout << "Leaf = " << ((*blatt)->value) << endl;	

	*blatt = leaf3;

	cout << "change leaf*" <<endl;
	SVLeaf** blatt2 = reinterpret_cast<SVLeaf**>(findChild(root, 0x01));
	cout << "Leaf = " << ((*blatt2)->value) << endl;	
*/
	

	insert(root, k, leaf, 0);
	insert(root, k2, leaf2, 0);
	insert(root, k3, leaf3, 0);

	cout << "Leaf = " << static_cast<SVLeaf*>(search(root, k, 0))->value << endl;
	cout << "Leaf2 = " << static_cast<SVLeaf*>(search(root, k2, 0))->value << endl;
	cout << "Leaf3 = " << static_cast<SVLeaf*>(search(root, k3, 0))->value << endl;

	insert(root, k4, leaf4, 0);
	cout << "Leaf4 = " << static_cast<SVLeaf*>(search(root, k4, 0))->value << endl;
/*
cout <<"Root - node4" << endl;
	Node4* root_Node4 = static_cast<Node4*>(root);
	for_each(root_Node4->keys.begin(), root_Node4->keys.end(), [](uint8_t key){ cout << (int)key << endl; });
	for_each(root_Node4->child.begin(), root_Node4->child.end(), [](BaseNode* ptr){ cout << ptr << endl; });
cout <<"root.child[0] - node4" << endl;
	Node4* child0_Node4 = static_cast<Node4*>(root_Node4->child[0]);
	for_each(child0_Node4->keys.begin(), child0_Node4->keys.end(), [](uint8_t key){ cout << (int)key << endl; });
	for_each(child0_Node4->child.begin(), child0_Node4->child.end(), [](BaseNode* ptr){ cout << ptr << endl; });
cout <<"root.child[0].child[0]" << endl;
	Node4* child00_Node4 = static_cast<Node4*>(child0_Node4->child[0]);
	//cout << "PrefixLen = " << (int)child00_Node4->prefixLen << endl;
//	for(int i = 0; i < (int)child00_Node4->prefixLen;i++)
//		cout << "Prefix["<<i<<"] = " << (int)child00_Node4->prefix[i] << endl;
	for_each(child00_Node4->keys.begin(), child00_Node4->keys.end(), [](uint8_t key){ cout << (int)key << endl; });
	for_each(child00_Node4->child.begin(), child00_Node4->child.end(), [](BaseNode* ptr){ cout << ptr << endl; });
*/	
	//auto root_Node48 = static_pointer_cast<SVLeaf>(root);
	//for_each(root_Node48->child.begin(), root_Node48->child.end(), [](shared_ptr<BaseNode> ptr){ cout << ptr << endl; });

	return 0;
}
