/**
 * @file dl_chain_list.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief Tool for double-link chain list.
 * @date 2019-03-01
 *
 * @copyright Copyright (c) 2019
 *
 */

/**
 * @name Standard C header include
 *
 */
#include <stdlib.h>

/**
 * @name Local header include
 *
 */
#include <dl_chain_list.h>

/**
 * @name Internal functions' prototype
 *
 */
static DL_NODE *dl_node_create( const void * );

/**
 * @brief Appending the new data to the chain list.
 *
 * @param head The head pointer of the chain list.
 * @param data The data that want to append to the chain list.
 * @return DL_NODE*
 * @retval NULL  - The node created failed or we can't find the head of the chain list.
 * @retval !NULL - The data appended successfully.
 */
DL_NODE *dl_node_append( DL_NODE **head, const void *data )
{
	DL_NODE  *prev    = NULL;
	DL_NODE **current = head;

/* */
	if ( current == (DL_NODE **)NULL )
		return NULL;
/* */
	while ( *current != (DL_NODE *)NULL ) {
		prev    = *current;
		current = &(*current)->next;
	}
/* */
	*current = dl_node_create( data );
	if ( *current != (DL_NODE *)NULL )
		(*current)->prev = prev;

	return *current;
}

/**
 * @brief Inserting the new data to the chain list.
 *
 * @param node The node pointer of in the chain list.
 * @param data The data that want to append to the chain list.
 * @return DL_NODE*
 * @retval NULL  - The node created failed or we can't find the node.
 * @retval !NULL - The data inserted successfully.
 */
DL_NODE *dl_node_insert( DL_NODE *node, const void *data )
{
	DL_NODE *new  = NULL;
	DL_NODE *next = NULL;

/* */
	if ( node != (DL_NODE *)NULL ) {
		next = node->next;
		new  = dl_node_create( data );
	/* */
		if ( new != (DL_NODE *)NULL ) {
			node->next = new;
			new->prev  = node;
			new->next  = next;
		/* */
			if ( next != (DL_NODE *)NULL )
				next->prev = new;
		}
	}

	return new;
}

/**
 * @brief Pushing the new data to the front of the chain list.
 *
 * @param head The head pointer of the chain list.
 * @param data The data that want to append to the chain list.
 * @return DL_NODE*
 * @retval NULL  - The node created failed or we can't find the head.
 * @retval !NULL - The data pushed successfully.
 */
DL_NODE *dl_node_push( DL_NODE **head, const void *data )
{
	DL_NODE *new = NULL;

/* */
	if ( head != (DL_NODE **)NULL ) {
		new = dl_node_create( data );
	/* */
		if ( new != (DL_NODE *)NULL ) {
			new->next = *head;
		/* */
			if ( *head != NULL )
				(*head)->prev = new;
			*head = new;
		}
	}

	return new;
}

/**
 * @brief Popping the first node of the chain list.
 *
 * @param head The head pointer of the chain list.
 * @return DL_NODE*
 * @retval NULL  - The node created failed or we can't find the head.
 * @retval !NULL - The node popped successfully.
 */
DL_NODE *dl_node_pop( DL_NODE **head )
{
	DL_NODE *current = NULL;

/* */
	if ( head != (DL_NODE **)NULL ) {
		current = *head;
	/* */
		if ( current != (DL_NODE *)NULL ) {
			*head = current->next;
		/* */
			if ( *head != NULL )
				(*head)->prev = NULL;
		/* Just close the chain of the pop-out node */
			current->prev = current->next = current;
		}
	}

	return current;
}

/**
 * @brief Deleting the node from the chain list.
 *
 * @param node The node pointer we want to delete.
 * @param func The function that will be execute before the node is deleted really.
 * @return DL_NODE*
 * @retval NULL  - The node deleted failed or we can't find the node.
 * @retval !NULL - The node deleted successfully.
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
		if ( prev != (DL_NODE *)NULL )
			prev->next = next;
		if ( next != (DL_NODE *)NULL )
			next->prev = prev;
	/**/
		if ( node->data != NULL && func != NULL )
			func( node->data );
		free(node);
	}

	return next;
}

/**
 * @brief Extracting the data pointer from the single node & free this node.
 *
 * @param node The node pointer of the single node.
 * @return void*
 * @retval NULL  - There is not any data inside this node.
 * @retval !NULL - The data extracted successfully.
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

/**
 * @brief Walking through the chain list and do the action.
 *
 * @param head The head pointer of the chain list.
 * @param action The action function want to apply to the nodes.
 * @param arg The action function's argument pointer.
 * @par Returns
 * 	Nothing.
 */
void dl_list_walk( DL_NODE *head, void (*action)( void *, const int, void * ), void *arg )
{
	DL_NODE *current = head;
	void    *data    = NULL;
	int      i       = 0;
/* */
	while ( current != (DL_NODE *)NULL ) {
	/* */
		data    = current->data;
		current = current->next;
		action( data, i++, arg );
	}

	return;
}

/**
 * @brief Walking through the chain list and pick out some nodes.
 *
 * @param head The head pointer of the chain list.
 * @param condition The judging function. If return <> 0, the node will be picked out; if return equal to zero,
 *                  the node will be keep in the list.
 * @param arg The judging function's argument pointer.
 * @param func The function that will be execute before the node is deleted really.
 * @return DL_NODE*
 * @retval NULL  - We can't find the head of the chain list or there isn't any node in the list.
 * @retval !NULL - It should be the head of the list.
 */
DL_NODE *dl_list_filter( DL_NODE **head, int (*condition)( void *, void * ), void *arg, void (*func)( void * ) )
{
	DL_NODE **current = head;
	void     *data    = NULL;

/* */
	if ( current == (DL_NODE **)NULL )
		return NULL;
/* */
	while ( *current != (DL_NODE *)NULL ) {
	/* */
		data = (*current)->data;
	/* */
		if ( data && condition( data, arg ) )
			*current = dl_node_delete( *current, func );
		else
			current = &(*current)->next;
	}

	return *head;
}

/**
 * @brief Destroying the whole double-link list.
 *
 * @param head The head pointer of the chain list.
 * @param func The function that will be execute before the node is deleted really.
 * @par Returns
 * 	Nothing.
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

/**
 * @brief Creating the new double-link node.
 *
 * @param data The data that want to add in this node.
 * @return DL_NODE*
 * @retval NULL  - The node created failed or we can't find the data.
 * @retval !NULL - The node created successfully.
 */
static DL_NODE *dl_node_create( const void *data )
{
	DL_NODE *new = NULL;

/* */
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
