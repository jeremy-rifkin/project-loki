#ifndef LINKEDLIST_H
#define LINKEDLIST_H

/*
	Doubly linked list implementaiton
*/

template<class T> class listNode {
private:
	listNode<T>* previousNode;
	listNode<T>* nextNode;
	T content;
public:
	listNode();
	listNode(T content);
	T getContent();
	void setContent(T content);
	listNode<T>* getPrevious();
	listNode<T>* getNext();
	void setPrevious(listNode<T>* previous);
	void setNext(listNode<T>* next);
};

template<class T> class list {
private:
	listNode<T>* head;
	listNode<T>* end;
	int length;
public:
	list();
	~list();
	listNode<T>* add(T content);
	void add(listNode<T>* node);
	void remove(listNode<T>* node);
	listNode<T>* insert(T content, listNode<T>* at);
	void insert(listNode<T>* node, listNode<T>* at);
	void clear();
	listNode<T>* getHead();
	listNode<T>* getEnd();
	int getLength();
};

#endif // LINKEDLIST_H
