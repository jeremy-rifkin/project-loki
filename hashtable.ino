#include "hashtable.h"

/*
	Hash table implementation. Creates hash table of linked lists.
*/

template<class T> hashTable<T>::hashTable(int nBins) {
	this->nBins = nBins;
	bins = new list<tableEntry<T>*>[nBins];
}

template<class T> hashTable<T>::~hashTable() {
	delete bins;
}

template<class T> T* hashTable<T>::get(int key) {
	// Get bin based off of hash
	list<tableEntry<T>*>* bin = &bins[hash(key)];
	// Traverse bin looking for key
	listNode<tableEntry<T>*>* node = bin->getHead();
	tableEntry<T>* content;
	if(node != null)
		do {
			content = node->getContent();
			if(content->key == key)
				return &content->value;
		} while(node = node->getNext());
	return null;
}

template<class T> tableEntry<T>* hashTable<T>::getEntry(int key) {
	// Get bin based off of hash
	list<tableEntry<T>*>* bin = &bins[hash(key)];
	// Traverse bin looking for key
	listNode<tableEntry<T>*>* node = bin->getHead();
	tableEntry<T>* content;
	if(node != null)
		do {
			content = node->getContent();
			if(content->key == key)
				return content;
		} while(node = node->getNext());
	return null;
}

template<class T> void hashTable<T>::set(int key, T value) {
	// Get bin based off of hash
	list<tableEntry<T>*>* bin = &bins[hash(key)];
	// Check to see if key is already present
	listNode<tableEntry<T>*>* node = bin->getHead();
	tableEntry<T>* content;
	if(node != null)
		do {
			content = node->getContent();
			if(content->key == key) {
				// If key is present just set new value
				content->value = value;
				return;
			}
		} while(node = node->getNext());
	// Otherwise add new key
	bin->add(new tableEntry<T>(key, value));
}

template<class T> void hashTable<T>::remove(int key) {
	list<tableEntry<T>*>* bin = &bins[hash(key)];
	listNode<tableEntry<T>*>* node = bin.getHead();
	tableEntry<T>* content;
	if(node != null)
		do {
			content = &node->getContent();
			if(content->key == key) {
				bin.remove(node);
				return;
			}
		} while(node = node->getNext());
}

template<class T> void hashTable<T>::clear() {
	for(int i = 0; i < nBins; i++)
		bins[i].clear();
}

template<class T> list<tableEntry<T>*>* hashTable<T>::getBins() {
	return bins;
}

template<class T> list<int>* hashTable<T>::getKeys() {
	list<int>* keys = new list<int>;
	listNode<tableEntry<T>*>* node;
	tableEntry<T>* content;
	for(int i = 0; i < nBins; i++) {
		node = bins[i].getHead();
		if(node != null)
			do
				keys->add(node->getContent()->key);
			while(node = node->getNext());
	}
	return keys;
}

template<class T> int hashTable<T>::getNBins() {
	return nBins;
}

template<class T> int hashTable<T>::hash(uint32_t key) {
	#if _MULTIPLICATIVE_HASH
	uint32_t p = key * 2654435761;
	return p % ((uint32_t)nBins);
	#else
	key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = (key >> 16) ^ key;
	return key % nBins;
	#endif
}
