/********************************************************************************
 * Author:		liuxg.
 * Date:		2017.02.17
 * Description:	binary semaphore.
 ********************************************************************************/
#ifndef BSEM_H
#define BSEM_H

//------------------------------------------------------------
//declare.
struct st_bsem;
typedef st_bsem bsem_t;

//------------------------------------------------------------
//@function: create a binary semaphore.
//@param value: 0 or 1.
//@return: ptr to created bsem_t.
bsem_t * bsem_create(int value);

//@function: post semaphore signal to one waitting thread.
//@param bsem_p: ptr to bsem_t.
void bsem_post(bsem_t * bsem_p);

//@function: post semaphore signal to all waitting threads.
//@param bsem_p: ptr to bsem_t.
void bsem_post_all(bsem_t * bsem_p);

//@function: wait signal from semaphore.
//@param bsem_p: ptr to bsem_t.
void bsem_wait(bsem_t * bsem_p);

//@function: reset binary semaphore to 0.
//@param bsem_p: ptr to bsem_t.
void bsem_reset(bsem_t * bsem_p);

//@function: destroy binary semaphore.
//@param bsem_p: ptr to bsem_t.
void bsem_destroy(bsem_t * bsem_p);

#endif //BSEM_H
