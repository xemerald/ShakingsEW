/*
 *
 */

/* Standard C header include */
#include <stdlib.h>
/**/
#include <dl_chain_list.h>

/* Internal functions' prototypes */
static DL_NODE *dl_node_create( const void * );

/*
 *  dl_node_append() - Appending the new data to the chain list.
 *  argument:
 *    head - The head pointer of the chain list.
 *    data - The data that want to append to the chain list.
 *  return:
 *    NULL  - The node created failed or we can't find the head of the chain list.
 *    !NULL - The data appended successfully.
 */
DL_NODE *dl_node_append( DL_NODE **head, const void *data )
{
	DL_NODE  *prev    = NULL;
	DL_NODE **current = head;

/**/
	if ( current == (DL_NODE **)NULL ) return NULL;
/**/
	while ( *current != (DL_NODE *)NULL ) {
		prev    = *current;
		current = &(*current)->next;
	}
/**/
	*current = dl_node_create( data );
	if ( *current != (DL_NODE *)NULL ) (*current)->prev = prev;

	return *current;
}

/*
 *  dl_node_insert() - Inserting the new data to the chain list.
 *  argument:
 *    node - The node pointer of in the chain list.
 *    data - The data that want to append to the chain list.
 *  return:
 *    NULL  - The node created failed or we can't find the node.
 *    !NULL - The data inserted successfully.
 */
DL_NODE *dl_node_insert( DL_NODE *node, const void *data )
{
	DL_NODE *new  = NULL;
	DL_NODE *next = NULL;

/**/
	if ( node != (DL_NODE *)NULL ) {
		next = node->next;
		new  = dl_node_create( data );
	/**/
		if ( new != (DL_NODE *)NULL ) {
			node->next = new;
			new->prev  = node;
			new->next  = next;
		/**/
			if ( next != (DL_NODE *)NULL ) next->prev = new;
		}
	}

	return new;
}

/*
 *  dl_node_push() - Pushing the new data to the front of the chain list.
 *  argument:
 *    head - The head pointer of the chain list.
 *    data - The data that want to append to the chain list.
 *  return:
 *    NULL  - The node created failed or we can't find the head.
 *    !NULL - The data pushed successfully.
 */
DL_NODE *dl_node_push( DL_NODE **head, const void *data )
{
	DL_NODE *new = NULL;

/**/
	if ( head != (DL_NODE **)NULL ) {
		new = dl_node_create( data );
	/**/
		if ( new != (DL_NODE *)NULL ) {
			new->next = *head;
		/**/
			if ( *head != NULL ) (*head)->prev = new;
			*head = new;
		}
	}

	return new;
}

/*
 *  dl_node_pop() - Popping the first node of the chain list.
 *  argument:
 *    head - The head pointer of the chain list.
 *  return:
 *    NULL  - The node created failed or we can't find the head.
 *    !NULL - The node popped successfully.
 */
DL_NODE *dl_node_pop( DL_NODE **head )
{
	DL_NODE *current = NULL;

/**/
	if ( head != (DL_NODE **)NULL ) {
		current = *head;
	/**/
		if ( current != (DL_NODE *)NULL ) {
			*head = current->next;
		/**/
			if ( *head != NULL ) (*head)->prev = NULL;
		/* Just close the chain of the pop-out node */
			current->prev = current->next = current;
		}
	}

	return current;
}

/*
 *  dl_node_delete() - Deleting the node from the chain list.
 *  argument:
 *    node - The node pointer we want to delete.
 *    func - The function that will be execute before the node is deleted really.
 *  return:
 *    NULL  - The node deleted failed or we can't find the node.
 *    !NULL - The node deleted successfully.
 */
DL_NODE *dl_node_delete( DL_NODE *node, void (*func)( void * ) )
{
	DL_NODE *prev = NULL;
	DL_NODE *next = NULL;

/**/
	if ( node != (DL_NODE *)NULL ) {
		prev = node->prev;
		next = node->next;
	/**/
		if ( prev != (DL_NODE *)NULL ) prev->next = next;
		if ( next != (DL_NODE *)NULL ) next->prev = prev;
	/**/
		if ( node->data != NULL && func != NULL ) func(node->data);
		free(node);
	}

	return next;
}

/*
 *  dl_node_data_extract() - Extracting the data pointer from the single node & free this node.
 *  argument:
 *    node - The node pointer of the single node.
 *  return:
 *    NULL  - There is not any data inside this node.
 *    !NULL - The data extracted successfully.
 */

void *dl_node_data_extract( DL_NODE *node )
{
	void *data = NULL;

/**/
	if ( node != (DL_NODE *)NULL ) {
	/**/
		if ( node->prev == node->next ) {
			data = node->data;
			free(node);
		}
	}

	return data;
}

/*
 *  dl_list_destroy() - Destroying the whole double-link list.
 *  argument:
 *    head - The head pointer of the chain list.
 *    func - The function that will be execute before the node is deleted really.
 *  return:
 *    None.
 */
void dl_list_destroy( DL_NODE **head, void (*func)( void * ) )
{
	DL_NODE *current = NULL;

/**/
	if ( head != (DL_NODE **)NULL ) {
		current = *head;
	/**/
		while ( current != (DL_NODE *)NULL )
			current = dl_node_delete( current, func );
		*head = NULL;
	}

	return;
}

/*
 *  dl_node_create() - Creating the new double-link node.
 *  argument:
 *    data - The data that want to add in this node.
 *  return:
 *    NULL  - The node created failed or we can't find the data.
 *    !NULL - The node created successfully.
 */
static DL_NODE *dl_node_create( const void *data )
{
	DL_NODE *new = NULL;

/**/
	if ( data != NULL ) {
		new = (DL_NODE *)malloc(sizeof(DL_NODE));
		if ( new != (DL_NODE *)NULL ) {
			new->data = (void *)data;
			new->prev = NULL;
			new->next = NULL;
		}
	}

	return new;
}
