/*************************************************************************
 > File Name: list_head.h
 > Desc: wrap dobly linked list
 > Author: tiankonguse(skyyuan)
 > Mail: i@tiankonguse.com 
 > Created Time: 2016年06月25日
 ***********************************************************************/
 
#include "doubly_linked_list.h"

#ifndef _LIST_HEAD_H_
#define _LIST_HEAD_H_


class CListHead{
public:
    struct list_head objlist;

    void InitList(void) {
        INIT_LIST_HEAD(&objlist);
    }
    void ResetList(void) {
        list_del_init(&objlist);
    }
    int ListEmpty(void) const {
        return list_empty(&objlist);
    }
    CListHead *ListNext(void) {
        return list_entry(objlist.next, CListHead, objlist);
    }
    CListHead *ListPrev(void) {
        return list_entry(objlist.prev, CListHead, objlist);
    }

    void ListAdd(CListHead &n) {
        list_add(&objlist, &n.objlist);
    }
    void ListAdd(CListHead *n) {
        list_add(&objlist, &n->objlist);
    }
    void ListAddTail(CListHead &n) {
        list_add_tail(&objlist, &n.objlist);
    }
    void ListAddTail(CListHead *n) {
        list_add_tail(&objlist, &n->objlist);
    }
    void ListDel(void) {
        ResetList();
    }
    void ListMove(CListHead &n) {
        list_move(&objlist, &n.objlist);
    }
    void ListMove(CListHead *n) {
        list_move(&objlist, &n->objlist);
    }
    void ListMoveTail(CListHead &n) {
        list_move_tail(&objlist, &n.objlist);
    }
    void ListMoveTail(CListHead *n) {
        list_move_tail(&objlist, &n->objlist);
    }
    void FreeList(void) {
        while (!ListEmpty()){
            ListNext()->ResetList();
        }  
    }
};


template<class T>
class CListObject: public CListHead{
public:
    CListObject(void) {
        InitList();
    }
    ~CListObject(void) {
        ResetList();
    }
    CListObject<T> *ListNext(void) {
        return (CListObject<T>*)CListHead::ListNext();
    }
    CListObject<T> *ListPrev(void) {
        return (CListObject<T>*)CListHead::ListPrev();
    }
    // T *ListOwner(void) { return static_cast<T *>(this); }
    T *ListOwner(void) {
        return (T *)this;
    }
    T *NextOwner(void) {
        return ListNext()->ListOwner();
    }
    T *PrevOwner(void) {
        return ListPrev()->ListOwner();
    }
};


#endif
