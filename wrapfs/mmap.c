/*
 * Copyright (c) 1998-2017 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2017 Stony Brook University
 * Copyright (c) 2003-2017 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "wrapfs.h"



int wrapfs_write_lower(struct inode *wrapfs_inode, char *data,
			 loff_t offset, size_t size)
{
	struct file *lower_file;
	ssize_t rc;
  struct dentry *tmp_dentry = NULL;
  char * buff = NULL;
  char * path_of_file;

  tmp_dentry = d_find_alias(wrapfs_inode);  
  path_of_file = dentry_path_raw(tmp_dentry, buff, PATH_MAX);
    
 
 lower_file = filp_open(path_of_file, O_RDWR, 00700);
 	if (!lower_file)
		return -EIO;
   
	rc = kernel_write(lower_file, data, size, offset);
	mark_inode_dirty_sync(wrapfs_inode);
	return rc;
}

/**
 * wrapfs_write_lower_page_segment
 * @wrapfs_inode: The wrapfs inode
 * @page_for_lower: The page containing the data to be written to the
 *                  lower file
 * @offset_in_page: The offset in the @page_for_lower from which to
 *                  start writing the data
 * @size: The amount of data from @page_for_lower to write to the
 *        lower file
 *
 * Determines the byte offset in the file for the given page and
 * offset within the page, maps the page, and makes the call to write
 * the contents of @page_for_lower to the lower inode.
 *
 * Returns zero on success; non-zero otherwise
 */
int wrapfs_write_lower_page_segment(struct inode *wrapfs_inode,
				      struct page *page_for_lower,
				      size_t offset_in_page, size_t size)
{
	char *virt;
	loff_t offset;
	int rc;

  // calculates the offset
	offset = ((((loff_t)page_for_lower->index) << PAGE_SHIFT)
		  + offset_in_page);
        
 // gets the virtual address
	virt = kmap(page_for_lower);
 
	rc = wrapfs_write_lower(wrapfs_inode, virt, offset, size);
	if (rc > 0)
		rc = 0;

// need to unmap immediately since only limited virtual addresses available
	kunmap(page_for_lower);
	return rc;
}


/*
encrypts the page and writes the page to disk
*/
int encrypt_page(struct page *page){
  struct inode *wrapfs_inode;
  int rc = 0;
  
  wrapfs_inode = page->mapping->host;
  
  rc = wrapfs_write_lower_page_segment(wrapfs_inode, page, 0, PAGE_SIZE);
  if (rc < 0) {
		printk(KERN_ERR
			"Error attempting to write lower page; rc = [%d]\n",
			rc);
		
	}

 return rc; 
}


/*
Writes page to the disk.
*/
static int wrapfs_writepage(struct page *page, struct writeback_control *wbc){
  int rc;
  
  printk(KERN_INFO "WRITING PAGE ");  
  rc = encrypt_page(page);
  
  if(rc){
    printk(KERN_WARNING "Error encrypting "
				"page (upper index [0x%.16lx])\n", page->index);
        ClearPageUptodate(page);
        goto out;
  }
  
  SetPageUptodate(page);
  
  out:
    unlock_page(page);
    return rc;

}


/*
  Reads the page from the disk.
*/
static int wrapfs_readpage(struct file *file, struct page *page){
  char *virt;
  loff_t offset;
  int rc = 0;
  struct file * lower_file;
  
  printk(KERN_INFO "MMAP: READING PAGE ");
    
  offset = ((((loff_t)(page->index)) << PAGE_SHIFT) + 0);
  virt = kmap(page);
  
  // gets the lower file
  lower_file = wrapfs_file_to_lower(file);
  if (!lower_file)
    return -EIO;

  printk(KERN_INFO "MMAP: READING LOWER FILE");

  rc= kernel_read(lower_file, offset, virt, PAGE_SIZE);
  if (rc < 0)
		rc = 0;
    
  kunmap(page);

  // flushes the cache page
	flush_dcache_page(page); 
  
  if (!rc) {
				printk(KERN_ERR "Error reading page; rc = "
				       "[%d]\n", rc);
				goto out;
			}
      
 out:
	if (!rc)
		{
			ClearPageUptodate(page);
			printk(KERN_INFO "MMAP: SUCCESS READING PAGE");
		}
	else{
		  SetPageUptodate(page);
		 printk(KERN_INFO "MMAP: FAILED TO READ PAGE");
		}


	printk(KERN_DEBUG "Unlocking page with index = [0x%.16lx] , and rc = %d\n",
			page->index, rc);

	unlock_page(page);
	return rc;
}


static sector_t xcfs_bmap(struct address_space *mapping, sector_t block)
{
	int rc = 0;
	struct inode *inode;
	struct inode *lower_inode;

	inode = (struct inode *)mapping->host;
	lower_inode = wrapfs_lower_inode(inode);
	if (lower_inode->i_mapping->a_ops->bmap)
		rc = lower_inode->i_mapping->a_ops->bmap(lower_inode->i_mapping,
							 block);
	return rc;
}

const struct address_space_operations wrapfs_aops = {
 //.writepage = wrapfs_writepage,
  .readpage= wrapfs_readpage,
//  .bmap = xcfs_bmap,
};



