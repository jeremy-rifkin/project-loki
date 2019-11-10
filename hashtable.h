#include "list.h"

#ifndef HASHTABLE_H
#define HASHTABLE_H

/*
	Hash table implementation. Creates hash table of linked lists.
*/

#define _MULTIPLICATIVE_HASH true // I think knuth's multiplicative method will work well but want to test further....

template<class T> struct tableEntry {
	tableEntry(int key, T value) {
		this->key = key;
		this->value = value;
	}
	int key;
	T value;
};

template<class T> class hashTable {
private:
	list<tableEntry<T>*>* bins;
	int nBins;
	int keys;
	int hash(int key);
public:
	hashTable(int nBins);
	~hashTable();
	T* get(int key);
	void set(int key, T value);
	void remove(int key);
	void clear();
	list<tableEntry<T>*>* getBins();
	list<int>* getKeys();
	int getNBins();
};

#endif
