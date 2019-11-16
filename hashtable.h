#include "list.h"

#ifndef HASHTABLE_H
#define HASHTABLE_H

/*
	Hash table with linear probing
*/

#define _MULTIPLICATIVE_HASH true // I think knuth's multiplicative method will work well but want
									// to test further....

enum tableEntryState : uint8_t { empty, alloc, unalloc };

template<class T> struct tableEntry {
	tableEntryState state = empty;
	int key;
	T value;
};

template<class T> class hashTable {
private:
	tableEntry<T>* bins;
	int nBins;
	int filled;
public:
	hashTable(int nBins);
	~hashTable();
	T get(int key);
	tableEntry<T>* getEntry(int key);
	void set(int key, T value);
	void remove(int key);
	void clear();
	tableEntry<T>* getBins();
	int getKeys(int* arr);
	int getNBins();
	int getFilled();
	int hash(uint32_t key);
	T operator[](int key);
};

#endif
