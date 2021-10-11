/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdio.h>
#include <memory.h>

#include "common.h"
#include "../index.h"
#include "../cpages.h"

// Проверяем индекс размера дырки (макросы HOLESIZE_IDX и HOLESIZE_CNT)
int hole_size_idx_test(char *errbuf)
	{
	unsigned needed_hs_idx = 0, hs_idx,i;
	// От 1 до MAX_KEY_SIZE должно увеличиваться на 1
	for (i = 1; i <= MAX_KEY_SIZE; i++)
		{
		hs_idx = HOLESIZE_IDX(i);
		if (needed_hs_idx != hs_idx)
			{ 
			sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",i,hs_idx,needed_hs_idx);
			return 1; 
			}
		needed_hs_idx++;
		}
	// Пока старший бит тот же что и в MAX_KEY_SIZE - должно увеличиваться на 1
	unsigned size = 1;
	while ((size <<= 1) <= MAX_KEY_SIZE);
	for (; i < size; i++)
		{
		hs_idx = HOLESIZE_IDX(i);
		if (needed_hs_idx != hs_idx)
			{ 
			sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",i,hs_idx,needed_hs_idx); 
			return 1; 
			}
		needed_hs_idx++;
		}
	// Дальше до PAGE_SIZE должно увеличиваться на 1 с каждым новым старшим битом
	while (size < PAGE_SIZE)
		{
		hs_idx = HOLESIZE_IDX(size);
		if (needed_hs_idx != hs_idx)
			{ 
			sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",size,hs_idx,needed_hs_idx); 
			return 1; 
			}
		hs_idx = HOLESIZE_IDX(size * 2 - 1);
		if (needed_hs_idx != hs_idx)
			{ 
			sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",size,hs_idx,needed_hs_idx);
			return 1; 
			}
		needed_hs_idx++;
		size *= 2;
		}
	hs_idx = HOLESIZE_IDX(size);
	if (needed_hs_idx != hs_idx)
		{ sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",size,hs_idx,needed_hs_idx); return 1; }
	if (++hs_idx != HOLESIZE_CNT)
		{ sprintf(errbuf,"Bad holesize count, has %d, should be %d",hs_idx,HOLESIZE_CNT); return 1; }
	return 0;	
	}

// Проверка выделения/освобождения страниц (idx_alloc_page,idx_free_page)
int page_alloc_test(FSingSet *index,int *reserved_memory,element_type prep_data)
	{
	element_type *page;
	unsigned pnum;
	idx_alloc_page(index,PT_GENERAL);
	idx_alloc_page(index,PT_GENERAL);
	if (index->head->pcnt != 3) // Проверим что обе страницы выделились
		{ idx_set_formatted_error(index,"Bad total allocated pages count, has %d, should be 3",index->head->pcnt); return 1; }
	idx_free_page(index,1);
	idx_free_page(index,2);
	if (index->head->first_empty_page != 2) // Проверим что в заголовке ссылка на последнюю освобожденную страницу
		{ idx_set_formatted_error(index,"Bad first empty page, has %d, should be 2",index->head->first_empty_page); return 1; }
	idx_alloc_page(index,PT_GENERAL);
	if (index->head->first_empty_page != 1) // Проверим что предыдущая освобожденная стала первой по цепочке
		{ idx_set_formatted_error(index,"Bad first empty page, has %d, should be 1",index->head->first_empty_page); return 1; }
	idx_alloc_page(index,PT_GENERAL);
	if (index->head->first_empty_page != NO_PAGE) // Проверим что ссылка на первую свободную обнулилась
		{ idx_set_formatted_error(index,"Bad first empty page, has %d, should be %d",index->head->first_empty_page,NO_PAGE); return 1; }
	pnum = idx_alloc_page(index,PT_GENERAL); 
	// Проверим что обе ранее освобожденные использованы и выделяется новое место
	if (index->head->pcnt != 4) 
		{ idx_set_formatted_error(index,"Bad total allocated pages count, has %d, should be 3",index->head->pcnt); return 1; }
	if (pnum != 3) 
		{ idx_set_formatted_error(index,"Bad allocated page number, has %d, should be 3",pnum); return 1; }
	page = index->pages[pnum];
	if (page[PAGE_SIZE - 1]) // Проверим что выделенное место обнулено и мы можем читать оттуда
		{ idx_set_formatted_error(index,"Data at allocated page 3 is not zeroes"); return 1; }
	*reserved_memory += PAGE_SIZE * 3;
	return 0;	
	}

// Маленькие дырки

#define SMALL_TEST_SIZE (MIN_HOLE_SIZE - 1)

element_type *test_small_alloc(FSingSet *index,unsigned size,element_type *ref_pointer,int *res_mem)
	{
	*res_mem += size;
	return idx_small_alloc(index,size,ref_pointer);
	}

void test_small_free(FSingSet *index,element_type data_idx,unsigned size,int *res_mem)
	{
	*res_mem -= size;
	idx_small_free(index,data_idx,size);
	}

element_type fill_first_subpage(FSingSet *index,unsigned reserv,int *res_mem)
	{
	unsigned i;
	element_type ref;
	unsigned holes_count = (DISK_PAGE_SIZE - SPEC_PAGE_HEAD_SIZE - SUB_PAGE_HEAD_SIZE) / SMALL_TEST_SIZE;
	for (i = 0; i < holes_count - reserv; i++)
		test_small_alloc(index,SMALL_TEST_SIZE,&ref,res_mem);
	return ref;
	}

element_type fill_subpage(FSingSet *index,unsigned reserv,int *res_mem)
	{
	unsigned i;
	element_type ref;
	unsigned holes_count = (DISK_PAGE_SIZE - SUB_PAGE_HEAD_SIZE) / SMALL_TEST_SIZE;
	for (i = 0; i < holes_count - reserv; i++)
		test_small_alloc(index,SMALL_TEST_SIZE,&ref,res_mem);
	return ref;
	}

element_type free_subpage(FSingSet *index,element_type ref,unsigned reserv,int *res_mem)
	{
	unsigned i;
	unsigned pnum = ref >> PAGE_SHIFT;
	unsigned spnum = (ref & OFFSET_MASK) / DISK_PAGE_SIZE;
	unsigned spstart = pnum * PAGE_SIZE + (spnum ? (spnum * DISK_PAGE_SIZE) : SPEC_PAGE_HEAD_SIZE) + SUB_PAGE_HEAD_SIZE;
	unsigned holes_count = (DISK_PAGE_SIZE - (spnum ? SUB_PAGE_HEAD_SIZE : (SUB_PAGE_HEAD_SIZE + SPEC_PAGE_HEAD_SIZE))) / SMALL_TEST_SIZE;
	for (i = reserv; i < holes_count; i++)
		test_small_free(index,spstart + i * SMALL_TEST_SIZE,SMALL_TEST_SIZE,res_mem);
	return spstart;
	}

//(1) Маленькое выделение
//(1.1) Есть подстраница с дырками
//	1_1_1 Это единственная дырка на ней
// 1_1_2 Это не единственная дырка на ней
//(1.2) Нет дырок но есть частично выделенная подстраница
//	1_2_1 В ней остается место после выделения
//	1_2_2 В ней не остается места после выделения
//(1.3) Нет дырок и нет частично выделенной подстраницы
//	1_3_1 Нет спецстраницы с пустыми подстраницами (начальный случай)
//(1.3.2) Есть спецстраница с пустыми подстраницами
//	1_3_2_1 Это последняя пустая подстраница на ней
// 1_3_2_2 Это не последняя пустая подстраница на ней

int small_alloc_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	element_type ref;
	cp_mark_head_dirty(index->used_cpages);
	return test_small_alloc(index,SMALL_TEST_SIZE,&ref,res_mem),0;
	}

int small_alloc_test_no_head(FSingSet *index,int *res_mem,element_type prep_data)
	{
	element_type ref;
	return test_small_alloc(index,SMALL_TEST_SIZE,&ref,res_mem),0;
	}

element_type small_alloc_prep_1_1_1(FSingSet *index,int *res_mem)
	{
	element_type ref = fill_first_subpage(index,0,res_mem) - SMALL_TEST_SIZE;
	test_small_free(index,ref,SMALL_TEST_SIZE,res_mem);
	return 0;
	}

element_type small_alloc_prep_1_1_2(FSingSet *index,int *res_mem)
	{
	element_type ref = fill_first_subpage(index,0,res_mem) - SMALL_TEST_SIZE * 2;
	test_small_free(index,ref,SMALL_TEST_SIZE,res_mem);
	test_small_free(index,ref + SMALL_TEST_SIZE,SMALL_TEST_SIZE,res_mem);
	return 0;
	}

element_type small_alloc_prep_1_2_1(FSingSet *index,int *res_mem)
	{ return fill_first_subpage(index,2,res_mem),0; }

element_type small_alloc_prep_1_2_2(FSingSet *index,int *res_mem)
	{ return fill_first_subpage(index,1,res_mem),0; }

element_type small_alloc_prep_1_3_2_1(FSingSet *index,int *res_mem)
	{ 
	unsigned i;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 63; i++)
		fill_subpage(index,0,res_mem);
	return 0;
	}

element_type small_alloc_prep_1_3_2_2(FSingSet *index,int *res_mem)
	{ return fill_first_subpage(index,0,res_mem),0; }

//(2) Маленькое освобождение
//(2.1) Это последний занятый элемент на подстранице
//(2.1.1) На подстранице есть дырки
// 2_1_1_1 Подстраница первая в цепочке
//(2.1.1.2) Подстраница не первая в цепочке
// 2_1_1_2_1 Подстраница не последняя в цепочке
// 2_1_1_2_2 Подстраница последняя в цепочке
// 2_1_2 На подстранице нет дырок (элемент единственный) 
// 2_1_3 Это частично выделенная подстраница (проверяется тестом 2_1_2)
// 2_1_4 Это не частично выделенная подстраница (проверяется тестом 2_1_1_2_1)
//(2.1.5) Вся страница распределена на подстраницы
// 2_1_5_1 Есть другие частично выделенные страницы
// 2_1_5_2 Нет других частично выделенных страниц
//(2.1.6) Это единственная выделенная подстраница на странице
// 2_1_6_1 Это первая в цепочке спецстраница с пустыми подстраницами (проверяется тестом 2_1_1_1)
//(2.1.6.2) Это не первая в цепочке спецстраница с пустыми подстраницами
// 2_1_6_2_1 Есть следующая
// 2_1_6_2_2 Нет следующей
// 2_1_7 Это не единственная выделенная подстраница на странице, но страница имеет пустые подстраницы (проверяется тестом 2_1_1_2_2)
//(2.2) Это не последний занятый элемент на подстранице
// 2_2_1 Он примыкает к зоне выделения для размера
//(2.2.2) Он не примыкает к зоне выделения для размера
//(2.2.2.1) На подстранице нет дырок
// 2_2_2_1_1 Есть другие подстраницы с дырками
// 2_2_2_1_2 Нет других подстраниц с дырками
// 2_2_2_2 На подстранице есть дырки

int small_free_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_small_free(index,prep_data,SMALL_TEST_SIZE,res_mem),0; 
	}

int small_free_test_no_head(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return test_small_free(index,prep_data,SMALL_TEST_SIZE,res_mem),0; 
	}

// также покрывает случай 2_1_6_1
element_type small_free_prep_2_1_1_1(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	test_small_free(index,hole1,SMALL_TEST_SIZE,res_mem);
	return hole2;
	}

// также покрывает случай 2_1_4
element_type small_free_prep_2_1_1_2_1(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2,hole3;
	hole1 = fill_first_subpage(index,0,res_mem);
	hole2 = fill_subpage(index,0,res_mem);
	hole3 = fill_subpage(index,0,res_mem) - SMALL_TEST_SIZE;
	test_small_free(index,hole1,SMALL_TEST_SIZE,res_mem);
	hole2 = free_subpage(index,hole2,1,res_mem);
	test_small_free(index,hole3,SMALL_TEST_SIZE,res_mem);
	return hole2;
	}

// также покрывает случай 2_1_7
element_type small_free_prep_2_1_1_2_2(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2,hole3;
	hole1 = fill_first_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole3,res_mem);
	test_small_free(index,hole2,SMALL_TEST_SIZE,res_mem);
	test_small_free(index,hole1,SMALL_TEST_SIZE,res_mem);
	return hole3;
	}

// также покрывает случай 2_1_3
element_type small_free_prep_2_1_2(FSingSet *index,int *res_mem)
	{ 
	element_type hole1;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem);
	return hole1;
	}

element_type small_free_prep_2_1_5_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	element_type hole,hole2;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		hole = fill_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	return free_subpage(index,hole,1,res_mem);
	}

element_type small_free_prep_2_1_5_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	element_type hole;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		hole = fill_subpage(index,0,res_mem);
	return free_subpage(index,hole,1,res_mem);
	}

element_type small_free_prep_2_1_6_2_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	element_type subpages[64];
	element_type hole,hole2,hole3;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		hole = fill_subpage(index,0,res_mem);
	subpages[0] = fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		subpages[i] = fill_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole3,res_mem);
	for (i = 1; i < 64; i++)
		free_subpage(index,subpages[i],0,res_mem);
	hole2 = free_subpage(index,subpages[0],1,res_mem);
	free_subpage(index,hole,0,res_mem);
	return hole2;
	}

element_type small_free_prep_2_1_6_2_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	element_type hole,hole2;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		hole = fill_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	free_subpage(index,hole,0,res_mem);
	return hole2;
	}

element_type small_free_prep_2_2_1(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	return hole2;
	}

element_type small_free_prep_2_2_2_1_1(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2,hole3;
	hole1 = fill_first_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole3,res_mem);
	test_small_free(index,hole1,SMALL_TEST_SIZE,res_mem);
	return hole2;
	}

element_type small_free_prep_2_2_2_1_2(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	return hole1;
	}

element_type small_free_prep_2_2_2_2(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2,hole3;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole3,res_mem);
	test_small_free(index,hole2,SMALL_TEST_SIZE,res_mem);
	return hole1;
	}

// Большие дырки, цепочки

void _index_hole(FSingSet *index,element_type data_idx,unsigned size);
void _deindex_hole(FSingSet *index,element_type hole_idx,unsigned hs_idx,FHoleHeader *hole);

// Т.к. функции индексации дырок для оптимизации не отмечают измененности заголовка, это надо делать извне
// Также в тестах дырки создаются на пустом пространстве первой страницы, это место будет посчитано два раза
// и нужна коррекция через res_mem
void test_index_hole(FSingSet *index,element_type data_idx,unsigned size,int *res_mem)
	{
	*res_mem -= size;
	_index_hole(index,data_idx,size);
	cp_mark_head_dirty(index->used_cpages);
	}

void test_deindex_hole(FSingSet *index,element_type hole_idx,unsigned size,int *res_mem)
	{
	*res_mem += size;
	unsigned hidx = HOLESIZE_IDX(size);
	if (index->head->holes[hidx] == hole_idx)
		cp_mark_head_dirty(index->used_cpages);
	_deindex_hole(index,hole_idx,hidx,(FHoleHeader *)regionPointer(index,hole_idx,size));
	}

//(3) Индексация дырки
//	3_1 Не пустая цепочка для размера
//(3.2) Пустая цепочка для размера
//	3_2_1 Индекс размера >= 64
//	3_2_2 Индекс размера < 64

element_type hole_index_prep_3_1(FSingSet *index,int *res_mem)
	{ return test_index_hole(index,1,MIN_HOLE_SIZE,res_mem),0; }

int hole_index_test_3_1(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_index_hole(index,1 + MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem),0; }

int hole_index_test_3_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_index_hole(index,1,PAGE_SIZE - 2,res_mem),0; }

int hole_index_test_3_2_2(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_index_hole(index,1,MIN_HOLE_SIZE,res_mem),0; }

//(4) Деиндексация дырки
//(4.1) Дырка первая в цепочке
//(4.1.1) Дырка последняя в цепочке
//	4_1_1_1 Индекс размера >= 64 
//	4_1_1_2 Индекс размера < 64
//	4_1_2 Дырка не последняя в цепочке
//(4.2) Дырка не первая в цепочке
//	4_2_1 Дырка последняя в цепочке
//	4_2_2 Дырка не последняя в цепочке

int hole_deindex_test_small(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_deindex_hole(index,prep_data,MIN_HOLE_SIZE,res_mem),0; }

int hole_deindex_test_big(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_deindex_hole(index,prep_data,PAGE_SIZE - 2,res_mem),0; }

element_type hole_index_prep_4_1_1_1(FSingSet *index,int *res_mem)
	{ return test_index_hole(index,1,PAGE_SIZE - 2,res_mem),1; }

element_type hole_index_prep_4_1_1_2(FSingSet *index,int *res_mem)
	{ return test_index_hole(index,1,MIN_HOLE_SIZE,res_mem),1; }

element_type hole_index_prep_4_1_2(FSingSet *index,int *res_mem)
	{ 
	test_index_hole(index,1,MIN_HOLE_SIZE,res_mem); 
	test_index_hole(index,1 + MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem);
	return 1; 
	}

element_type hole_index_prep_4_2_1(FSingSet *index,int *res_mem)
	{ 
	test_index_hole(index,1,MIN_HOLE_SIZE,res_mem); 
	test_index_hole(index,1 + MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem);
	return 1; 
	}

element_type hole_index_prep_4_2_2(FSingSet *index,int *res_mem)
	{ 
	test_index_hole(index,1,MIN_HOLE_SIZE,res_mem); 
	test_index_hole(index,1 + MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem);
	test_index_hole(index,1 + 2 * MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem);
	return 1 + MIN_HOLE_SIZE; 
	}

// Большие дырки, выделение/освобождение

// Выделяем память и имитируем запись туда каких-то данных
element_type test_large_alloc(FSingSet *index,unsigned size,int *res_mem)
	{
	element_type rv; 
	element_type *space = idx_large_alloc(index,size,&rv);
	if (rv == ZERO_REF) return rv;
	space = PAGES_POINTER(index,rv);
	space[0] = space[size - 1] = INVALID_REF; // ZERO_REF не подходит, т.к. может быть записан в это место при освобождении
	unsigned i;
	for (i = 1; i < size - 1; i++)
		space[i]++;
	*res_mem += size;
	return rv;
	}

void test_large_free(FSingSet *index,element_type ref,unsigned size,int *res_mem)
	{
	idx_large_free(index,ref,size);
	element_type *space = PAGES_POINTER(index,ref);
	if (size > HOLE_HEADER_SIZE + HOLE_FOOTER_SIZE) 
		memset(&space[HOLE_HEADER_SIZE],0,(size - HOLE_HEADER_SIZE - HOLE_FOOTER_SIZE) * ELEMENT_SIZE);
	*res_mem -= size;
	}

//(5) Большое выделение
// 5_1 Есть дырка точного размера
//(5.2) Нет дырки точного размера, есть дырка >= +MIN_HOLE_SIZE размера
// 5_2_1 Индекс искомого размера >= 64
//(5.2.2) Индекс искомого размера < 64
//	5_2_2_1 Индекс найденного размера >= 64
//	5_2_2_2 Индекс найденного размера < 64
//(5.2.3) 
//	5_2_3_1 Индекс искомого размера <= INDEXED_HOLESIZE_CNT (проверяется 5_2_2_2)
//	5_2_3_2 Индекс искомого размера > INDEXED_HOLESIZE_CNT
//(5.3) Нет подходящей дырки
//	5_3_1 Остаток места в неразмеченной области >= требуемому размеру (начальный случай)
//	5_3_2 Остаток места в неразмеченной области < треб. размера и >= MIN_HOLE_SIZE
//	5_3_3 Остаток места в неразмеченной области < MIN_HOLE_SIZE

int gen_alloc_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_alloc(index,MIN_HOLE_SIZE,res_mem),0; 
	}

element_type gen_alloc_prep_5_1(FSingSet *index,int *res_mem)
	{
	element_type ref = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref,MIN_HOLE_SIZE,res_mem);
	return 0;
	}

element_type gen_alloc_prep_large_small(FSingSet *index,int *res_mem)
	{
	element_type ref = test_large_alloc(index,PAGE_SIZE - MIN_HOLE_SIZE * 3 - 1,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE * 3,res_mem);
	test_large_free(index,ref,PAGE_SIZE - MIN_HOLE_SIZE * 3 - 1,res_mem);
	return 0;
	}

int gen_alloc_test_5_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_alloc(index,PAGE_SIZE / 2 - MIN_HOLE_SIZE * 2,res_mem),0;
	}

element_type gen_alloc_prep_5_2_2_2(FSingSet *index,int *res_mem)
	{
	element_type ref = test_large_alloc(index,2 * MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref,2 * MIN_HOLE_SIZE,res_mem);
	return 0;
	}

int gen_alloc_test_5_2_3_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_alloc(index,PAGE_SIZE - MIN_HOLE_SIZE * 2,res_mem),0;
	}

element_type gen_alloc_prep_5_3_2(FSingSet *index,int *res_mem)
	{ return test_large_alloc(index,PAGE_SIZE - MIN_HOLE_SIZE - 1,res_mem),0; }

int gen_alloc_test_5_3_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_alloc(index,MIN_HOLE_SIZE * 2,res_mem),0; 
	}

element_type gen_alloc_prep_5_3_3(FSingSet *index,int *res_mem)
	{
	*res_mem += 1; // Неиндексируемый остаток места на странице (т.к. данных реально нет, он не найдется)
	return test_large_alloc(index,PAGE_SIZE - 2,res_mem),0; 
	}

//(6) Большое освобождение
//(6.1) Смещение на странице >= MIN_HOLE_SIZE 
// 6_1_1 Ниже дырка
// 6_1_2 Ниже нет дырки
// 6_2 Смещение на странице < MIN_HOLE_SIZE (проверяется тестом 6_1_2)
//(6.3) Примыкает к неразмеченной области
// 6_3_1 Находится в начале страницы (проверяется тестом 6_1_2)
// 6_3_2 Находится не в начале страницы
//(6.4) Не примыкает к неразм. области
// 6_4_1 Остаток страницы < MIN_HOLE_SIZE
//(6.4.2) Остаток страницы >= MIN_HOLE_SIZE
// 6_4_2_1 Дальше нет дырки
// 6_4_2_2 Дальше дырка
// 6_4_3 Новая дырка не примыкает к неразм. области и размер равен странице 
// 6_4_4 Новая дырка не примыкает к неразм. области и размер меньше странице (проверяется тестом 6_4_1)

int gen_free_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_free(index,prep_data,MIN_HOLE_SIZE,res_mem),0; 
	}

element_type gen_free_prep_6_1_1(FSingSet *index,int *res_mem)
	{
	element_type ref1,ref2;
	ref1 = test_large_alloc(index,MIN_HOLE_SIZE,res_mem); 
	ref2 = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref1,MIN_HOLE_SIZE,res_mem);
	return ref2;
	}

element_type gen_free_prep_6_1_2(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1,res_mem); 
	return test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	}

element_type gen_free_prep_6_3_2(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1,res_mem); 
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	return test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	}

element_type gen_free_prep_6_4_1(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1 - MIN_HOLE_SIZE - 1,res_mem); 
	element_type rv = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	return rv;
	}

element_type gen_free_prep_6_4_2_1(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1 - MIN_HOLE_SIZE * 2,res_mem); 
	element_type rv = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	return rv;
	}

element_type gen_free_prep_6_4_2_2(FSingSet *index,int *res_mem)
	{
	element_type rv = test_large_alloc(index,MIN_HOLE_SIZE,res_mem); 
	element_type ref = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref,MIN_HOLE_SIZE,res_mem);
	return rv;
	}

element_type gen_free_prep_6_4_3(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1,res_mem); 
	element_type rv = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	element_type ref = test_large_alloc(index,PAGE_SIZE - MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref,MIN_HOLE_SIZE,res_mem);
	return rv;
	}

// Общие функции

int main(void)
	{
	char errbuf[512] = {0};
	int rv = 0;
	if (hole_size_idx_test(errbuf))
		printf("Test hole_size_idx failed: %s\n",errbuf),rv = 1;

	FTestData tests[] = {
		{"page_alloc",NULL,page_alloc_test,LM_NONE,0},

		{"small_alloc_1_1_1",small_alloc_prep_1_1_1,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_1_2",small_alloc_prep_1_1_2,small_alloc_test_no_head,LM_NONE,0},
		{"small_alloc_1_2_1",small_alloc_prep_1_2_1,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_2_2",small_alloc_prep_1_2_2,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_3_1",NULL,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_3_2_1",small_alloc_prep_1_3_2_1,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_3_2_2",small_alloc_prep_1_3_2_1,small_alloc_test,LM_NONE,0},

		{"small_free_2_1_1_1",small_free_prep_2_1_1_1,small_free_test,LM_NONE,0},
		{"small_free_2_1_1_2_1",small_free_prep_2_1_1_2_1,small_free_test_no_head,LM_NONE,0},
		{"small_free_2_1_1_2_2",small_free_prep_2_1_1_2_2,small_free_test,LM_NONE,0},
		{"small_free_2_1_2",small_free_prep_2_1_2,small_free_test,LM_NONE,0},
		{"small_free_2_1_5_1",small_free_prep_2_1_5_1,small_free_test,LM_NONE,0},
		{"small_free_2_1_5_2",small_free_prep_2_1_5_2,small_free_test,LM_NONE,0},
		{"small_free_2_1_6_2_1",small_free_prep_2_1_6_2_1,small_free_test,LM_NONE,0},
		{"small_free_2_1_6_2_2",small_free_prep_2_1_6_2_2,small_free_test,LM_NONE,0},
		{"small_free_2_2_1",small_free_prep_2_2_1,small_free_test,LM_NONE,0},
		{"small_free_2_2_2_1_1",small_free_prep_2_2_2_1_1,small_free_test,LM_NONE,0},
		{"small_free_2_2_2_1_2",small_free_prep_2_2_2_1_2,small_free_test,LM_NONE,0},
		{"small_free_2_2_2_2",small_free_prep_2_2_2_2,small_free_test_no_head,LM_NONE,0},

		{"hole_index_3_1",hole_index_prep_3_1,hole_index_test_3_1,LM_NONE,0},
		{"hole_index_3_2_1",NULL,hole_index_test_3_2_1,LM_NONE,0},
		{"hole_index_3_2_2",NULL,hole_index_test_3_2_2,LM_NONE,0},

		{"hole_index_4_1_1_1",hole_index_prep_4_1_1_1,hole_deindex_test_big,LM_NONE,0},
		{"hole_index_4_1_1_2",hole_index_prep_4_1_1_2,hole_deindex_test_small,LM_NONE,0},
		{"hole_index_4_1_2",hole_index_prep_4_1_2,hole_deindex_test_small,LM_NONE,0},
		{"hole_index_4_2_1",hole_index_prep_4_2_1,hole_deindex_test_small,LM_NONE,0},
		{"hole_index_4_2_2",hole_index_prep_4_2_2,hole_deindex_test_small,LM_NONE,0},

		{"gen_alloc_5_1",gen_alloc_prep_5_1,gen_alloc_test,LM_NONE,0},
		{"gen_alloc_5_2_1",gen_alloc_prep_large_small,gen_alloc_test_5_2_1,LM_NONE,0},
		{"gen_alloc_5_2_2_1",gen_alloc_prep_large_small,gen_alloc_test,LM_NONE,0},
		{"gen_alloc_5_2_2_2",gen_alloc_prep_5_2_2_2,gen_alloc_test,LM_NONE,0},
		{"gen_alloc_5_2_3_2",gen_alloc_prep_large_small,gen_alloc_test_5_2_3_2,LM_NONE,0},
		{"gen_alloc_5_3_1",NULL,gen_alloc_test,LM_NONE,0},
		{"gen_alloc_5_3_2",gen_alloc_prep_5_3_2,gen_alloc_test_5_3_2,LM_NONE,0},
		{"gen_alloc_5_3_3",gen_alloc_prep_5_3_3,gen_alloc_test,LM_NONE,0},

		{"gen_free_6_1_1",gen_free_prep_6_1_1,gen_free_test,LM_NONE,0},
		{"gen_free_6_1_2",gen_free_prep_6_1_2,gen_free_test,LM_NONE,0},
		{"gen_free_6_3_2",gen_free_prep_6_3_2,gen_free_test,LM_NONE,0},
		{"gen_free_6_4_1",gen_free_prep_6_4_1,gen_free_test,LM_NONE,0},
		{"gen_free_6_4_2_1",gen_free_prep_6_4_2_1,gen_free_test,LM_NONE,0},
		{"gen_free_6_4_2_2",gen_free_prep_6_4_2_2,gen_free_test,LM_NONE,0},
		{"gen_free_6_4_3",gen_free_prep_6_4_3,gen_free_test,LM_NONE,0},
		};
	
	unsigned i,tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}