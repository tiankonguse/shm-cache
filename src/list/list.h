/*************************************************************************
 > File Name: list.h
 > Desc: c++ doubly linked list
 > Author: tiankonguse(skyyuan)
 > Mail: i@tiankonguse.com 
 > Created Time: 2016年06月25日
 ***********************************************************************/

#ifndef _LIST_H_
#define _LIST_H_

struct List{
    struct List *prev;
    struct List *next;

    List(){ 
        Init(); 
    };
    
    ~List() { 
        Remove(); 
    };

    void Init(){
        next = this;
        prev = this;
    }

    static void Add(struct List *n, struct List *prev, struct List *next){
        next->prev = n;
        n->next = next;
        n->prev = prev;
        prev->next = n;
    }

    static void Remove(struct List *prev, struct List *next){
        next->prev = prev;
        prev->next = next;
    }

    void AddTail(struct List *head){
        Add(this, head->prev, head);
    }

    void Add(struct List *head){
        Add(this, head, head->next);
    }

    void Remove(){
        Remove(prev, next);
        Init();
    }

    void Reset(){
        Init();
    }

    bool IsEmpty(){
        return next == this && prev == this;
    }
private:
    struct List & operator= (const struct List &a);
};


#endif