#include "list.h"

template<class T> listNode<T>::listNode() {
	nextNode = null;
	content = null;
}

template<class T> listNode<T>::listNode(T content) {
	nextNode = null;
	setContent(content);
}

template<class T> T listNode<T>::getContent() {
	return content;
}

template<class T> void listNode<T>::setContent(T content) {
	this->content = content;
}

template<class T> bool listNode<T>::hasNext() {
	return nextNode != null;
}

template<class T> listNode<T>* listNode<T>::next() {
	return nextNode;
}

template<class T> void listNode<T>::setNext(listNode<T>* next) {
	nextNode = next;
}


template<class T> list<T>::list() {
	head = null;
	end = null;
}

template<class T> void list<T>::add(T content) {
	listNode<T> *node = new listNode<T>(content);
	add(node);
}

template<class T> void list<T>::add(listNode<T>* node) {
	if(head == null) {
		head = node;
		end = node;
	} else {
		end->setNext(node);
		end = node;
	}
}

template<class T> listNode<T>* list<T>::getHead() {
	return head;
}

template<class T> listNode<T>* list<T>::getEnd() {
	return end;
}
