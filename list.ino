#include "list.h"

/*
	Doubly linked list implementaiton
*/

template<class T> listNode<T>::listNode() {
	previousNode = null;
	nextNode = null;
	content = null;
}

template<class T> listNode<T>::listNode(T content) {
	previousNode = null;
	nextNode = null;
	this->content = content;
}

template<class T> T listNode<T>::getContent() {
	return content;
}

template<class T> void listNode<T>::setContent(T content) {
	this->content = content;
}

template<class T> listNode<T>* listNode<T>::getPrevious() {
	return previousNode;
}


template<class T> listNode<T>* listNode<T>::getNext() {
	return nextNode;
}

template<class T> void listNode<T>::setPrevious(listNode<T>* previous) {
	previousNode = previous;
}

template<class T> void listNode<T>::setNext(listNode<T>* next) {
	nextNode = next;
}


template<class T> list<T>::list() {
	head = null;
	end = null;
	length = 0;
}

template<class T> list<T>::~list() {
	// Unallocate all listNodes
	listNode<T>* node = head;
	listNode<T>* next;
	while(node != null) {
		next = node->getNext();
		delete node;
		node = next;
	}
}

template<class T> listNode<T>* list<T>::add(T content) {
	listNode<T> *node = new listNode<T>(content);
	add(node);
	return node;
}

template<class T> void list<T>::add(listNode<T>* node) {
	if(head == null) {
		head = node;
		end = node;
	} else {
		node->setPrevious(end);
		end->setNext(node);
		end = node;
	}
	length++;
}

template<class T> void list<T>::remove(listNode<T>* node) {
	listNode<T>* previous = node->getPrevious();
	listNode<T>* next = node->getNext();
	if(previous == null)
		head = next;
	else
		previous->setNext(next);
	if(next == null)
		end = previous;
	else
		next->setPrevious(previous);
	delete node;
	length--;
}

template<class T> listNode<T>* list<T>::insert(T content, listNode<T>* at) {
	listNode<T> *node = new listNode<T>(content);
	insert(node, at);
	return node;
}

template<class T> void list<T>::insert(listNode<T>* node, listNode<T>* at) {
	if(head == null) {
		head = node;
		end = node;
	} else {
		if(at == end)
			end = node;
		node->setPrevious(at);
		node->setNext(at->getNext());
		at->setNext(node);
		node->getNext()->setPrevious(node);
		length++;
	}
}

template<class T> void list<T>::clear() {
	listNode<T>* node = head;
	listNode<T>* _node;
	while(node != null) {
		_node = node;
		node = node->getNext();
		delete _node;
	}
	head = null;
	end = null;
	length = 0;
}

template<class T> listNode<T>* list<T>::getHead() {
	return head;
}

template<class T> listNode<T>* list<T>::getEnd() {
	return end;
}

template<class T> int list<T>::getLength() {
	return length;
}
