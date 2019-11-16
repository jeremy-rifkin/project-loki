#include "hashTable.h"

/*
	Hash table implementation. Creates hash table of linked lists.
*/

template<class T> hashTable<T>::hashTable(int nBins) {
	this->nBins = nBins;
	filled = 0;
	bins = new tableEntry<T>[nBins];
}

template<class T> hashTable<T>::~hashTable() {
	delete bins;
}

template<class T> T hashTable<T>::get(int key) {
	int index = hash(key);
	for(int i = 0; i < nBins; i++)
		if(bins[i].state == empty)
			break;
		else if(bins[i].state == alloc && bins[i].key == key)
			return bins[i].value;
		else
			index = (index + 1) % nBins;
	return null;
}

template<class T> tableEntry<T>* hashTable<T>::getEntry(int key) {
	int index = hash(key);
	for(int i = 0; i < nBins; i++)
		if(bins[i].state == empty)
			break;
		else if(bins[i].state == alloc && bins[i].key == key)
			return &bins[i];
		else
			index = (index + 1) % nBins;
	return null;
}

template<class T> void hashTable<T>::set(int key, T value) {
	int index = hash(key);
	for(int i = 0; i < nBins; i++)
		if(bins[i].key == key || bins[i].state != alloc) {
			if(bins[i].state != alloc)
				filled++;
			bins[i].state = alloc;
			bins[i].key = key;
			bins[i].value = value;
			break;
		} else
			index = (index + 1) % nBins;
	//throw "Key doesn't exist in hash table and there's no empty bins";
}

template<class T> void hashTable<T>::remove(int key) {
	int index = hash(key);
	for(int i = 0; i < nBins; i++)
		if(bins[i].state == alloc && bins[i].key == key) {
			bins[i].state = unalloc;
			filled--;
			// TODO: shift everything? Doesn't matter too much...
			break;
		} else
			index = (index + 1) % nBins;
}

template<class T> void hashTable<T>::clear() {
	for(int i = 0; i < nBins; i++)
		bins[i].state = empty;
	filled = 0;
}

template<class T> tableEntry<T>* hashTable<T>::getBins() {
	return bins;
}

template<class T> int hashTable<T>::getWork(int key) {
	// Returns work required to get / set key
	int w = 0;
	int index = hash(key);
	for(int i = 0; i < nBins; i++) {
		w++;
		if(bins[i].state == empty || (bins[i].state == alloc && bins[i].key == key))
			break;
		else
			index = (index + 1) % nBins;
	}
	return w;
}

template<class T> int hashTable<T>::getKeys(int* arr) {
	int j = 0;
	for(int i = 0; i < nBins; i++)
		if(bins[i].state == alloc)
			arr[j++] = bins[i].key;
	return j;
}

template<class T> int hashTable<T>::getNBins() {
	return nBins;
}

template<class T> int hashTable<T>::getFilled() {
	return filled;
}

template<class T> T hashTable<T>::operator[](int key) {
	return get(key);
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
