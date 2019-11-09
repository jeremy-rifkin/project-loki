#ifndef LINKEDLIST_H
#define LINKEDLIST_H

template<class T> class listNode {
private:
	listNode<T>* nextNode;
	T content;
public:
	listNode();
	listNode(T content);
	T getContent();
	void setContent(T content);
	bool hasNext();
	listNode<T>* next();
	void setNext(listNode<T>* next);
};

template<class T> class list {
private:
	listNode<T>* head;
	listNode<T>* end;
public:
	list();
	void add(listNode<T>* node);
	void add(T content);
	listNode<T>* getHead();
	listNode<T>* getEnd();
};

#endif // LINKEDLIST_H
