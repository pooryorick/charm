/*Charm++ Mesh Refinement Framework:
  C++ implementation file
  
  Orion Sky Lawlor, olawlor@acm.org, 4/8/2002
  Modified by Terry Wilmarth, wilmarth@cse.uiuc.edu, 4/16/2002
*/
#ifndef _UIUC_CHARM_REFINE_IMPL_H
#define _UIUC_CHARM_REFINE_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif
  
  // Create a new refinement object for this virtual processor.
  // Must be called exactly once at startup.
  void REFINE2D_Init(void);
  
  /**
   * Push a new mesh into the refinement system.  This is the first
   * call user programs make into the refinement system.  This call
   * need *not* be repeated when the refinement system changes the mesh;
   * only when the user changes the mesh (e.g., to coarsen it).
   *
   * Conn is row-major, and maps an element number to three node numbers.
   * Hence conn[i*3+j] gives the local number of node j of element i.
   * Because of shared nodes, every node of every local element will be local.
   * Ghost elements may not have the complete set of nodes-- some of their
   * nodes may have the invalid number -1.
   * 
   * Elements with numbers between 0 and nEl-1 (inclusive) are local.
   * Elements with numbers between nEl and nGhost-1 (inclusive) are "ghosts"--
   * elements that are actually local on another processor.  There are
   * guaranteed to be enough ghosts that every local element's non-boundary
   * edge will face a ghost element.
   *
   * gid maps an element number to a chunk number and local number on that
   * chunk.  These are stored at gid[i*2+0] (chunk number) and gid[i*2+1]
   * (local number).
   * 
   * boundaries is the boundary flags for nodes. boundaries may be null,
   * if the user doesnt specify boundary flags.
   * nnodes specifies the number of nodes 
   */
  void REFINE2D_NewMesh(int meshID, int nEl, int nGhost, int nnodes,
			const int *conn, const int *gid,
			const int *boundaries, const int *edgeBounds, 
			const int *edgeConn, int nEdges);
  
  /**
   * Refine the mesh so each element has, at most, the given desired area.
   * 
   * Coord gives the (x,y) coordinates of nodes 0..nNode-1.
   * coord[2*j+0] is the x coordinate of node j
   * coord[2*j+1] is the y coordinate of node j
   *
   * desiredArea[i] gives the desired area of element i.  nEl must be 
   * equal to the "nEl" value passed in via REFINE_NewMesh plus the number
   * of "split" calls received during earlier refinements.
   */
	 class FEM_Refine_Operation_Data;
  void REFINE2D_Split(int nNode, double *coord, int nEl, double *desiredArea,FEM_Refine_Operation_Data *refine_data);
 
	typedef struct{
    int tri,A,B,C,D, _new,origEdgeB, newEdge1B, newEdge2B; 
    double frac;
    int flag;
	} refineData;

 
  // Get the number of split triangles.
  int REFINE2D_Get_Split_Length(void);

  /**
   * Return one split triangle.
   * A and B are the nodes along the splitting edge:
   *
   *                     C                      C                 
   *                    / \                    /|\                  
   *                   /   \                  / | \                 
   *                  /     \      =>        /  |  \                
   *                 /  tri  \              /   |   \               
   *                /         \            /tri | new\            
   *               B --------- A          B --- D --- A         
   *
   *   The original triangle's node A should be replaced by D;
   * while a new triangle should be inserted with nodes CAD.
   *
   *   The new node D's location should equal A*(1-frac)+B*frac.
   * For a simple splitter, frac will always be 0.5.
   *
   *   If nodes A and B are shared with some other processor,
   * that processor will also receive a "split" call for the
   * same edge.  If nodes A and B are shared by some other local
   * triangle, that triangle will immediately receive a "split" call
   * for the same edge.  
   *
   * Parameters:
   *   -splitNo is the number of this split.  Pass splitNo
   *      in increasing order from 0 to REFINE2D_Get_Split_Length()-1.
   *   -conn is the triangle connectivity array, used to look up
   *      the node numbers A, B, and C.  This array is *not* modified.
   *   -tri returns the number of the old triangle being split;
   *      as labelled above.
   *   -A,B,C returns the numbers of the nodes in the above diagram.
   *   -frac returns the weighting for D between A and B;
   *     for now, this is always 0.5.
   *
   * Client's responsibilities:
   *   -Add the new node D.  Since both sides of a shared local edge
   *      will receive a "split" call, you must ensure the node is
   *      not added twice; you can do this by checking this split's
   *      nodes A and B against the previous split.
   *   -Update connectivity for source triangle
   *   -Add new triangle. 
   */
  void REFINE2D_Get_Split(int splitNo, refineData *d);


  // Data structures to store the changes caused by coarsening.  
  // Store the data for 1 collapse operation
  typedef struct {
    int elemID, nodeToKeep, nodeToDelete;
    double newX, newY;
    int flag;
    int boundaryFlag;
    double frac;
  } collapseData;
  
  // Store the data for 1 nodeUpdate operation
  typedef struct {
    int nodeID, boundaryFlag;
    double newX, newY;
  } updateData;
  
  // Store the data for 1 nodeReplaceDelete operation
  typedef struct {
    int elemID, relnodeID, oldNodeID, newNodeID;
  } replacedeleteData;
  
  // Store the data for a change caused by coarsening, might be of 3 types: 
  // collapse, updatenode or replace and delete a node
  enum {INVALID=0,COLLAPSE,UPDATE,REPLACE};
  typedef struct {
    int type;
    union {
      collapseData cdata;
      updateData udata;
      replacedeleteData rddata;
    } data;
  } coarsenData;
  
  class FEM_Operation_Data;
  void REFINE2D_Coarsen(int nNode, double *coord, int nEl, double *desiredArea,
			FEM_Operation_Data *data);
  
  /*void REFINE2D_Get_Collapse(int i,int *conn,int *tri,int *nodeToThrow,int *nodeToKeep,double *nx,double *ny,int *flag,int idxbase);*/
  
  void REFINE2D_Get_Collapse(int i, coarsenData *output);
  int REFINE2D_Get_Collapse_Length();
  
  // Check to make sure our connectivity and the refine connectivity agree.
  void REFINE2D_Check(int nEle, const int *conn, int nNodes);
  
#ifdef __cplusplus
};
#endif

#endif


