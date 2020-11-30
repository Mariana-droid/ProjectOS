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


	if (dirEntries == NULL) {


		return FAIL;
	}
	for (int i = 0; i < MAX_DIR_ENTRIES; i++) {
		if (dirEntries[i].inumber != FREE_INODE) {

			return FAIL;
		}
	}

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
	int array[MAX_DIR_ENTRIES] = {0}; /*Guardar o path em que fizemos locks*/
	int i = 0; /*tamanho do array*/
	int *n = &i;
	
	/* use for copy */
	type pType;
	union Data pdata;


	strcpy(name_copy, name);

	split_parent_child_from_path(name_copy, &parent_name, &child_name);

	parent_inumber = lookup_path(parent_name,&array[0],n); /*locks o path com reads e depois da write lock ao "pai"*/


	if (parent_inumber == FAIL) {
		printf("failed to create %s, invalid parent dir %s\n", name, parent_name);
		inode_rwlock_unlock(parent_inumber); /*unlocks antes de sair*/
		path_unlocker(&array[0],*n);

		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);
	
	if(pType != T_DIRECTORY) {
		printf("failed to create %s, parent %s is not a dir\n",
		        name, parent_name);
		inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/
		

		path_unlocker(&array[0],*n);

		return FAIL;
	}

	if (lookup_sub_node(child_name, pdata.dirEntries) != FAIL) {
		printf("failed to create %s, already exists in dir %s\n",
		       child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/



		path_unlocker(&array[0],*n);


		return FAIL;
	}

	/* create node and add entry to folder that contains new node */
	child_inumber = inode_create(nodeType);
	if (child_inumber == FAIL) {
		printf("failed to create %s in  %s, couldn't allocate inode\n",
		        child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/


		path_unlocker(&array[0],*n);

		return FAIL;
	}

	inode_rwlock_wrlock(child_inumber);


	if (dir_add_entry(parent_inumber, child_inumber, child_name) == FAIL) {
		printf("could not add entry %s in dir %s\n",
		       child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/

		inode_rwlock_unlock(child_inumber);

		path_unlocker(&array[0],*n);

		return FAIL;
	}
	inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/


	inode_rwlock_unlock(child_inumber);	

	path_unlocker(&array[0],*n);

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
	int array[MAX_DIR_ENTRIES]; /*array com o path*/
	int i =0;
	int *n = &i; /*length do array*/

	strcpy(name_copy, name);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);
	
	parent_inumber = lookup_path(parent_name,&array[0],n); /*altera o array e o n, devolve o parent inumber depois de fazer os locks*/

	if (parent_inumber == FAIL) {
		printf("failed to delete %s, invalid parent dir %s\n",
		        child_name, parent_name);
				inode_rwlock_unlock(parent_inumber); /*unlocks antes de sair*/
				path_unlocker(&array[0],*n);

		return FAIL;
	}

	inode_get(parent_inumber, &pType, &pdata);

	if(pType != T_DIRECTORY) {
		printf("failed to delete %s, parent %s is not a dir\n",
		        child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/
		path_unlocker(&array[0],*n);

		return FAIL;
	}

	child_inumber = lookup_sub_node(child_name, pdata.dirEntries);
	

	if (child_inumber == FAIL) {
		printf("could not delete %s, does not exist in dir %s\n",
		       name, parent_name);
		inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/
		path_unlocker(&array[0],*n);

		return FAIL;
	}
	inode_rwlock_wrlock(child_inumber);
	inode_get(child_inumber, &cType, &cdata);
	

	if (cType == T_DIRECTORY && is_dir_empty(cdata.dirEntries) == FAIL) {
		printf("could not delete %s: is a directory and not empty\n",
		       name);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(&array[0],*n);

		return FAIL;
	}

	/* remove entry from folder that contained deleted node */
	if (dir_reset_entry(parent_inumber, child_inumber) == FAIL) {
		printf("failed to delete %s from dir %s\n",
		       child_name, parent_name);
		inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/
		inode_rwlock_unlock(child_inumber);
		path_unlocker(&array[0],*n);

		return FAIL;
	}

	if (inode_delete(child_inumber) == FAIL ) {
		printf("could not delete inode number %d from dir %s\n",
		       child_inumber, parent_name);
		inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/
		inode_rwlock_unlock(child_inumber);
		path_unlocker(&array[0],*n);

		return FAIL;
	}
	
	inode_rwlock_unlock(parent_inumber);/*unlocks antes de sair*/
	inode_rwlock_unlock(child_inumber);
	path_unlocker(&array[0],*n);

	
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
/*
 * Lookup_path da locks e guarda no array o percurso do path.
 * Input:
 *  - name: path of node
 *  - array: guarda o path
 *  - n: guarda o length do array
 * Returns:
 *  inumber: identifier of the i-node, if found
 *     FAIL: otherwise
 * rlock
 */

int lookup_path(char *name, int *array, int *n){
	char full_path[MAX_FILE_NAME];
	char delim[] = "/";
	int i = 0;
	int aux;
	strcpy(full_path, name);

	/* start at root node */
	int current_inumber = FS_ROOT;

	/* use for copy */
	type nType;
	union Data data;

	inode_get(current_inumber, &nType, &data);

	char *path = strtok(full_path, delim);

		/* get root inode data */
	if( path == NULL || (lookup_sub_node(path, data.dirEntries) == FAIL)) /*Caso seja igual ao do root*/
	{
		*n = 0;
		inode_rwlock_wrlock(current_inumber); /*write lock e sair*/
		return current_inumber;
	}
	else 
	{
		inode_rwlock_rdlock(current_inumber); 
	}
	
	*array = current_inumber; /*aumentar a length*/
	i++;
	array++;

	aux = current_inumber;
	/* search for all sub nodes */
	while (path != NULL && (current_inumber = lookup_sub_node(path, data.dirEntries)) != FAIL) {
		inode_rwlock_rdlock(current_inumber);

		*array = current_inumber; /*guardar o numero*/
		i++;
		array++;
		inode_get(current_inumber, &nType, &data);
		aux = current_inumber; 
		path = strtok(NULL, delim);

	}
	inode_rwlock_unlock(aux); /*unlock do read seguido de um write lock*/
	inode_rwlock_wrlock(aux);

	i=i-1;
	if(i<0)
		i = 0; /*guardar zero caso seja -1*/
	*n = i;

	return aux;
}
/*
 * path_unlocker percorre o perurso e da unlock de tudo.
 * Input:
 *  - array: lista com os inumbers para dar unlock
 *  - n: length do array
 * Returns:
 *  nada
 */
void path_unlocker(int *array,int n){
	int i;
	if (n == 0) {
		return;
	} 
	for (i=0;i<n;i++){/*unlocks de todos os numeros do array*/
		inode_rwlock_unlock(*array);

		array++;

	}
}

/*
 * Move an input from one path to another
 * Input:
 *  - name1: path of node
 *  - name2: path of new place for node
 * Returns:
 *  Returns: SUCCESS or FAIL
 * 
 */

int move (char* name1,char* name2){
	int parent_inumber,parent_inumber2, child_inumber; 
	char *parent_name, *parent_name2, *child_name, name_copy[MAX_FILE_NAME];
	int array[MAX_DIR_ENTRIES] = {}; /*dois arrays, um para o name1 e outro para name2*/
	int array2[MAX_DIR_ENTRIES] = {};
	int i2 = 0; /*guardar a length dos dois*/
	int i = 0;
	int *n = &i;
	int *n2 = &i2;

	type pType,cType;
	union Data pdata,cdata;

	strcpy(name_copy, name1);
	split_parent_child_from_path(name_copy, &parent_name, &child_name);
	parent_inumber = lookup_path(parent_name,&array[0],n);
	/*locks do primeiro path*/
	if (parent_inumber == FAIL){
		printf("failed to move %s. invalid parent dir %s\n",name1,parent_name);
		inode_rwlock_unlock(parent_inumber);
		path_unlocker(&array[0],*n);
		return FAIL;
	}

	inode_get(parent_inumber,&pType,&pdata);

	if (pType != T_DIRECTORY){
		printf("failed to move %s, parent %s is not a dir\n",name1,parent_name);
		inode_rwlock_unlock(parent_inumber);
		path_unlocker(&array[0],*n);
		return FAIL;
	}
	child_inumber = lookup_sub_node(child_name,pdata.dirEntries);
	
	if (child_inumber == FAIL){
		printf("failed to move %s, doesn't exist in dir %s\n",child_name,parent_name);
		inode_rwlock_unlock(parent_inumber);
		path_unlocker(&array[0],*n);
		return FAIL;
	}
	inode_rwlock_wrlock(child_inumber);
	inode_get(child_inumber,&cType,&cdata);

	inode_rwlock_unlock(parent_inumber);

	strcpy(name_copy, name2);
	split_parent_child_from_path(name_copy, &parent_name2, &child_name);
	parent_inumber2 = lookup_path(parent_name2,&array2[0],n2); /*lock do segundo path*/

	if (parent_inumber2 == FAIL){
		printf("failed to move %s. invalid parent dir %s\n",name2,parent_name2);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(parent_inumber2);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(&array[0],*n);
		path_unlocker(&array2[0],*n);
		return FAIL;
	}

	inode_get(parent_inumber2,&pType,&pdata);

	if (pType != T_DIRECTORY){
		printf("failed to move %s, parent %s is not a dir\n",name2,parent_name2);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(parent_inumber2);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(&array[0],*n);
		path_unlocker(&array2[0],*n);
		return FAIL;
	}

	if (lookup_sub_node(child_name,pdata.dirEntries) != FAIL){ 
		printf("failed to move %s,it already exist in dir %s\n",child_name,parent_name2);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(parent_inumber2);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(&array[0],*n);
		path_unlocker(&array2[0],*n);
		return FAIL;
	}
	if (dir_reset_entry(parent_inumber,child_inumber) == FAIL){ /*tirar da diretoria anterior*/
		printf("failed to move %s from dir %s\n",child_name,parent_name);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(&array[0],*n);
		return FAIL;
	}

	if (dir_add_entry(parent_inumber2,child_inumber,child_name) == FAIL){ /*por na nova diretoria*/
		printf("could not add entry to %s in dir %s\n",child_name,parent_name2);
		inode_rwlock_unlock(parent_inumber);
		inode_rwlock_unlock(parent_inumber2);
		inode_rwlock_unlock(child_inumber);
		path_unlocker(&array[0],*n);
		path_unlocker(&array2[0],*n);
		return FAIL;
	}
	inode_rwlock_unlock(parent_inumber);
	inode_rwlock_unlock(parent_inumber2);
	inode_rwlock_unlock(child_inumber);
	path_unlocker(&array[0],*n);
	path_unlocker(&array2[0],*n);

	return SUCCESS;
}


/*
 * Prints tecnicofs tree.
 * Input:
 *  - fp: pointer to output file
 */
void print_tecnicofs_tree(FILE *fp){
	inode_print_tree(fp, FS_ROOT, "");
}
