#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/page-flags.h>
#include <asm/pgtable.h>
#include <asm/pgtable_types.h>
#include <linux/slab.h>

asmlinkage long sys_mm_dp_sc(unsigned long va)
{
	struct mm_struct *our_mm = current->mm;
	pgd_t *pgd_entry;
	pud_t *pud_entry;
	pmd_t *pmd_entry;
	pte_t *pt_entry;
	struct page *page_desc = NULL;
	struct vm_area_struct *vma = NULL;
	unsigned long total_len = 0;

	/* Process address space length*/
	if (our_mm && our_mm->mmap)
		for (vma = our_mm->mmap; vma; vma = vma->vm_next) {
			total_len += (vma->vm_end - vma->vm_start);
			printk(": vma start: 0x%16lx vma end: 0x%16lx size: %32ld\n",
					vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start);
		}

	printk("Total lenght of process address space: %ld\n", total_len);

	/* How to obtain page descriptor*/
	/* Get linear address of the entry in page global directory that corresponds to the given address. */
	pgd_entry = pgd_offset(our_mm, va);

	/* Check if the page global directory entry is valid or not*/
	if (pgd_none(*pgd_entry) || pgd_bad(*pgd_entry)) {
		printk(KERN_ERR "PGD invalid for virtual address %lx \n", va);
		goto invalid_addr;
	}

	/* Get linear address of the entry in page upper directory that corresponds to the given address. */
	pud_entry = pud_offset(pgd_entry, va);
	
	/* Check if the page upper directory entry is valid or not*/
	if (pud_none(*pud_entry) || pud_bad(*pud_entry)) {
		printk(KERN_ERR "PUD invalid for virtual address %lx \n", va);
		goto invalid_addr;
	}
	
	/* Get linear address of the entry in page middle directory that corresponds to the given address. */
	pmd_entry = pmd_offset(pud_entry, va);
	
	/* Check if the page middle directory entry is valid or not*/
	if (pmd_none(*pmd_entry) || pmd_bad(*pmd_entry)) {
		printk(KERN_ERR "PMD invalid for virtual address %lx \n", va);
		goto invalid_addr;
	}
	
	pt_entry = pte_offset_map(pmd_entry, va);
	if (pt_entry == NULL) {
		printk(KERN_ERR "PTE invalid for virtual address %lx \n", va);
		goto invalid_addr;
	}

	/* Page present in RAM or not */
	if (!pte_present(*pt_entry)) {
		printk(KERN_INFO "Address %lx present on RAM.\n", va);
		goto out;
	}
	
	printk(KERN_INFO "Address %lx present in RAM.\n", va);

	/* Page is dirty */
	if (pte_dirty(*pt_entry)) 
		printk(KERN_INFO "Page for address %lx present is dirty.\n", va);

	/* Page is referenced */
	page_desc = pte_page((*pt_entry));
	if (page_desc) {
		printk(KERN_INFO "Found page_desc %p for virtual address %lx\n", page_desc, va);
		if (page_desc->flags & 	PG_referenced)
			printk(KERN_INFO "Page for virtual address %lx was referenced\n", va);
	}

out:
	return 0;

invalid_addr:
	return -1;
}
