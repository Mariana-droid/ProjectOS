#include "operations.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Given a path, fills pointers with strings for the parent path and child
 * file name
 * Input:
 *  - path: the path to split. ATENTION: the function may alter this parameter
 *  - parent: reference to a char*, to store parent path
 *  - child: reference to a char*, to store child file name
 */
void split_parent_child_from_path(char * path, char ** parent, char ** child) {

	int n_slashes = 0, last_slash_location = 0;
	int len = strlen(path);

	// deal with trailing slash ( a/x vs a/x/ )
	if (path[len-1] == '/') {
		path[len-1] = '\0';
	}

	for (int i=0; i < len; ++i) {
		if (path[i] == '/' && path[i+1] != '\0') {
			last_slash_location = i;
			n_slashes++;
		}
	}

	if (n_slashes == 0) { // root directory
		*parent = "";
		*child = path;
		return;
	}

	path[last_slash_location] = '\0';
	*parent = path;
	*child = path + last_slash_location + 1;

}


/*
 * Initializes tecnicofs and creates root node.
 */
void init_fs() {
	inode_table_init();
	
	/* create root inode */
	int root = inode_create(T_DIRECTORY);
	
	if (root != FS_ROOT) {
		printf("failed to create node for tecnicofs root\n");
		exit(EXIT_FAILURE);
	}
}


/*
 * Destroy tecnicofs and inode table.
 */
void destroy_fs() {
	inode_table_destroy();
}


/*
 * Checks if content of directory is not empty.
 * Input:
 *  - entries: entries of directory
 * Returns: SUCCESS or FAIL
 * rlock only
 */

int is_dir_empty(DirEntry *dirEntries) {
		inode_rwlock_rdlock(dirEntries->inumber);

	if (dirEntries == NULL) {
	inode_rwlock_unlock(dirEntries->inumber);

		return FAIL;
	}
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
		if (dirEntries[i].inumber != FREE_INODE) {
			inode_rwlock_unlock(dirEntries->inumber);
			return FAIL;
		}
	}
	inode_rwlock_unlock(dirEntries->inumber);
	return SUCCESS;
}


/*
 * Looks for node in directory entry from name.
 * Input:
 *  - name: path of node
 *  - entries: entries of directory
 * Returns:
 *  - inumber: found node's inumber
 *  - FAIL: if not found
 * rlock
 */
int lookup_sub_node(char *name, DirEntry *entries) {
	

	if (entries == NULL) {
		
		return FAIL;
	}
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
		/*teste*/
        if (entries[i].inumber != FREE_INODE && strcmp(entries[i].name, name) == 0) {
            
			return entries[i].inumber;
        }
	}
	return FAIL;
}


/*
 * Creates a new node given a path.
 * Input:
 *  - name: path of node
 *  - nodeType: type of node
 * Returns: SUCCESS or FAIL
 * rwlock
 */
int create(char *name, type nodeType){
	
	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	int aux[MAX_DIR_ENTRIES];
	int *array = aux;
	int n;
	
	/* use for copy */
	type pType;
	union Data pdata;
	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup(parent_name);
	n = lookup_path(parent_name,array);

	inode_rwlock_wrlock(parent_inumber);
	if (parent_inumber == FAIL) {
		printf("failed to create %s, invalid parent dir %s\n",
		        name, parent_name);
		inode_rwlock_unlock(parent_inumber);
		path_unlocker(array,n);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);
	
	if(pType != T_DIRECTORY) {
		printf("failed to create %s, parent %s is not a dir\n",
		        name, parent_name);
		inode_rwlock_unlock(parent_inumber);
		path_unlocker(array,n);
		return FAIL;
	}

	if (lookup_sub_node(child_name, pdata.dirEntries) != FAIL) {
		printf("failed to create %s, already exists in dir %s\n",
		       child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);
		path_unlocker(array,n);
		return FAIL;
	}

	/* create node and add entry to folder that contains new node */
	child_inumber = inode_create(nodeType);
	if (child_inumber == FAIL) {
		printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);
		path_unlocker(array,n);
		return FAIL;
	}

	inode_rwlock_wrlock(child_inumber);

	if (dir_add_entry(parent_inumber, child_inumber, child_name) == FAIL) {
		printf("could not add entry %s in dir %s\n",
		       child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(array,n);
		return FAIL;
	}
	inode_rwlock_unlock(parent_inumber);
	inode_rwlock_unlock(child_inumber);	
	path_unlocker(array,n);

	return SUCCESS;
}


/*
 * Deletes a node given a path.
 * Input:
 *  - name: path of node
 * Returns: SUCCESS or FAIL
 * rwlock
 */
int delete(char *name){

	int parent_inumber, child_inumber;
	char *parent_name, *child_name, name_copy[MAX_FILE_NAME];
	/* use for copy */
	type pType, cType;
	union Data pdata, cdata;
	int aux[MAX_DIR_ENTRIES];
	int *array = aux;
	int n;
	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);
	parent_inumber = lookup(parent_name);
	n = lookup_path(parent_name,array);
	inode_rwlock_wrlock(parent_inumber);

	if (parent_inumber == FAIL) {
		printf("failed to delete %s, invalid parent dir %s\n",
		        child_name, parent_name);
				inode_rwlock_unlock(parent_inumber);
				path_unlocker(array,n);
		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to delete %s, parent %s is not a dir\n",
		        child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);
		path_unlocker(array,n);
		return FAIL;
	}

	child_inumber = lookup_sub_node(child_name, pdata.dirEntries);
	

	if (child_inumber == FAIL) {
		printf("could not delete %s, does not exist in dir %s\n",
		       name, parent_name);
		inode_rwlock_unlock(parent_inumber);
		path_unlocker(array,n);
		return FAIL;
	}
	inode_rwlock_wrlock(child_inumber);
	inode_get(child_inumber, &cType, &cdata);
	

	if (cType == T_DIRECTORY && is_dir_empty(cdata.dirEntries) == FAIL) {
		printf("could not delete %s: is a directory and not empty\n",
		       name);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(array,n);
		return FAIL;
	}

	/* remove entry from folder that contained deleted node */
	if (dir_reset_entry(parent_inumber, child_inumber) == FAIL) {
		printf("failed to delete %s from dir %s\n",
		       child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(array,n);
		return FAIL;
	}

	if (inode_delete(child_inumber) == FAIL) {
		printf("could not delete inode number %d from dir %s\n",
		       child_inumber, parent_name);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(array,n);
		return FAIL;
	}
	
	inode_rwlock_unlock(parent_inumber);
	inode_rwlock_unlock(child_inumber);
	path_unlocker(array,n);
	
	return SUCCESS;
}


/*
 * Lookup for a given path.
 * Input:
 *  - name: path of node
 * Returns:
 *  inumber: identifier of the i-node, if found
 *     FAIL: otherwise
 * rlock
 */

int lookup(char *name){
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";
	int inumbers[INODE_TABLE_SIZE];
	int i =0;

	strcpy(full_path, name);

	/* start at root node */
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	inumbers[i] = current_inumber;
	i++;
	/* get root inode data */
	inode_rwlock_rdlock(current_inumber);
	inode_get(current_inumber, &nType, &data);

	char *path = strtok(full_path, delim);

	/* search for all sub nodes */
	while (path != NULL && (current_inumber = lookup_sub_node(path, data.dirEntries)) != FAIL) {
		inode_rwlock_rdlock(current_inumber);
		inumbers[i] = current_inumber;
		i++;
		inode_get(current_inumber, &nType, &data);
		path = strtok(NULL, delim);
	}

	for (i=i-1;i>=0;i--){
		inode_rwlock_unlock(inumbers[i]);
	}
	return current_inumber;
}

int lookup_path(char *name,int *array){
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";
	int i =0;

	strcpy(full_path, name);

	/* start at root node */
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	array[i] = current_inumber;
	i++;
	/* get root inode data */
	inode_rwlock_rdlock(current_inumber);
	inode_get(current_inumber, &nType, &data);

	char *path = strtok(full_path, delim);

	/* search for all sub nodes */
	while (path != NULL && (current_inumber = lookup_sub_node(path, data.dirEntries)) != FAIL) {
		inode_rwlock_rdlock(current_inumber);
		array[i] = current_inumber;
		i++;
		inode_get(current_inumber, &nType, &data);
		path = strtok(NULL, delim);
	}
	inode_rwlock_unlock(current_inumber);
	return i -2;
}

void path_unlocker(int *array,int n){
	int i;
	for (i=0;i<=n;i++){
		inode_rwlock_unlock(array[i]);
	}
}
/*
 * Prints tecnicofs tree.
 * Input:
 *  - fp: pointer to output file
 */
void print_tecnicofs_tree(FILE *fp){
	inode_print_tree(fp, FS_ROOT, "");
}
