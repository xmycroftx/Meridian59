// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * roofile.c
 * 
 
 Server-side implementation of a ROO file.
 Loads the BSP tree like the client does and provides
 BSP queries on the tree, such as LineOfSightBSP or CanMoveInRoomBSP

 */

#include "blakserv.h"

#ifdef _MSC_VER
  #pragma warning (disable: 4244)// Switch off implicit type conversion warnings
#endif

#pragma region Macros
/*****************************************************************************************
********* macro functions ****************************************************************
*****************************************************************************************/
// distance from a point (b) to a BspInternal (a)
#define DISTANCETOSPLITTERSIGNED(a,b) ((a)->A * (b)->X + (a)->B * (b)->Y + (a)->C)

// floorheight of a point (b) in a sector (a)
#define SECTORHEIGHTFLOOR(a, b)	\
   ((!(a)->SlopeInfoFloor) ? (a)->FloorHeight : \
      (-(a)->SlopeInfoFloor->A * (b)->X \
       -(a)->SlopeInfoFloor->B * (b)->Y \
       -(a)->SlopeInfoFloor->D) / (a)->SlopeInfoFloor->C)

// ceilingheight of a point (b) in a sector (a)
#define SECTORHEIGHTCEILING(a, b)	\
   ((!(a)->SlopeInfoCeiling) ? (a)->CeilingHeight : \
      (-(a)->SlopeInfoCeiling->A * (b)->X \
       -(a)->SlopeInfoCeiling->B * (b)->Y \
       -(a)->SlopeInfoCeiling->D) / (a)->SlopeInfoCeiling->C)

/*****************************************************************************************
********* from clientd3d/draw3d.c ********************************************************
*****************************************************************************************/
#define DEPTHMODIFY0    0.0f
#define DEPTHMODIFY1    (ROOFINENESS / 5.0f)
#define DEPTHMODIFY2    (2.0f * ROOFINENESS / 5.0f)
#define DEPTHMODIFY3    (3.0f * ROOFINENESS / 5.0f)

#define ROOM_OVERRIDE_DEPTH1 0x00000001
#define ROOM_OVERRIDE_DEPTH2 0x00000002
#define ROOM_OVERRIDE_DEPTH3 0x00000004
#define ROOM_OVERRIDE_MASK   0x00000007

/*****************************************************************************************
********* from clientd3d/bsp.h ***********************************************************
*****************************************************************************************/
#define SF_DEPTH0          0x00000000      // Sector has default (0) depth
#define SF_DEPTH1          0x00000001      // Sector has shallow depth
#define SF_DEPTH2          0x00000002      // Sector has deep depth
#define SF_DEPTH3          0x00000003      // Sector has very deep depth
#define SF_MASK_DEPTH      0x00000003      // Gets depth type from flags
#define SF_SLOPED_FLOOR    0x00000400      // Sector has sloped floor
#define SF_SLOPED_CEILING  0x00000800      // Sector has sloped ceiling
#define SF_NOMOVE          0x00002000      // Sector can't be moved on by mobs or players
#define WF_TRANSPARENT     0x00000002      // normal wall has some transparency
#define WF_PASSABLE        0x00000004      // wall can be walked through
#define WF_NOLOOKTHROUGH   0x00000020      // bitmap can't be seen through even though it's transparent

// Sector flag change constants.
// Bitfield so multiple flags can be sent together.
// CSF_RESET_ALL sets to original flags.
#define CSF_DEPTH_RESET  0x00000001
#define CSF_DEPTH0       0x00000002
#define CSF_DEPTH1       0x00000003
#define CSF_DEPTH2       0x00000004
#define CSF_DEPTH3       0x00000005
#define CSF_DEPTHMASK    0x00000007
#define CSF_NOMOVE_RESET 0x00000008
#define CSF_NOMOVE_ON    0x00000010
#define CSF_NOMOVE_OFF   0x00000018
#define CSF_NOMOVEMASK   0x00000018
#define CSF_RESET_ALL    0x00000020
#pragma endregion

#pragma region Internal
/**************************************************************************************************************/
/*                                            INTERNAL                                                        */
/*                                   These are not defined in header                                          */
/**************************************************************************************************************/

// stores potential intersections during recursive CanMoveInRoom3D query
Intersections intersects;

__forceinline void BSPIntersectionsSwap(Intersection **a, Intersection **b)
{
   Intersection* t = *a; *a = *b; *b = t;
}

void BSPIntersectionsSort(Intersection* arr[], unsigned int beg, unsigned int end)
{
   // Recursive QuickSort implementation
   // sorting (potential) intersections by squared distance from start

   if (end > beg + 1)
   {
      Intersection* piv = arr[beg];
      unsigned int l = beg + 1, r = end;
      while (l < r)
      {
         const Intersection* itm = arr[l];
         if (itm->Distance2 < piv->Distance2 || (itm->Distance2 == piv->Distance2 && itm->FloorHeight < piv->FloorHeight))
            l++;
         else
            BSPIntersectionsSwap(&arr[l], &arr[--r]);
      }
      BSPIntersectionsSwap(&arr[--l], &arr[beg]);
      BSPIntersectionsSort(arr, beg, l);
      BSPIntersectionsSort(arr, r, end);
   }
}

__forceinline float BSPGetSectorHeightFloorWithDepth(const room_type* Room, const SectorNode* Sector, const V2* P)
{
   const float height = SECTORHEIGHTFLOOR(Sector, P);
   const unsigned int depthtype = Sector->Flags & SF_MASK_DEPTH;
   const unsigned int depthoverride = Room->DepthFlags & ROOM_OVERRIDE_MASK;

   if (depthtype == SF_DEPTH0)
      return (height - DEPTHMODIFY0);

   if (depthtype == SF_DEPTH1)
      return (depthoverride != ROOM_OVERRIDE_DEPTH1) ? (height - DEPTHMODIFY1) : Room->OverrideDepth1;

   if (depthtype == SF_DEPTH2)
      return (depthoverride != ROOM_OVERRIDE_DEPTH2) ? (height - DEPTHMODIFY2) : Room->OverrideDepth2;

   if (depthtype == SF_DEPTH3)
      return (depthoverride != ROOM_OVERRIDE_DEPTH3) ? (height - DEPTHMODIFY3) : Room->OverrideDepth3;

   return height;
}

__forceinline bool BSPCanMoveInRoomTreeInternal(const room_type* Room, const SectorNode* SectorS, const SectorNode* SectorE, const Side* SideS, const Side* SideE, const V2* Q)
{
   // block moves with end outside
   if (!SectorE || !SideE)
      return false;

   // don't block moves with start outside and end inside
   if (!SectorS || !SideS)
      return true;

   // sides which have no passable flag set always block
   if (!((SideS->Flags & WF_PASSABLE) == WF_PASSABLE))
      return false;

   // endsector must not be marked SF_NOMOVE
   if ((SectorE->Flags & SF_NOMOVE) == SF_NOMOVE)
      return false;

   // get floor heights
   const float hFloorS = BSPGetSectorHeightFloorWithDepth(Room, SectorS, Q);
   const float hFloorE = BSPGetSectorHeightFloorWithDepth(Room, SectorE, Q);

   // check stepheight (this also requires a lower texture set)
   if (SideS->TextureLower > 0 && (hFloorE - hFloorS > MAXSTEPHEIGHT))
      return false;

   // get ceiling height
   const float hCeilingE = SECTORHEIGHTCEILING(SectorE, Q);

   // check ceilingheight (this also requires an upper texture set)
   if (SideS->TextureUpper > 0 && (hCeilingE - hFloorS < OBJECTHEIGHTROO))
      return false;

   // check endsector height
   if (hCeilingE - hFloorE < OBJECTHEIGHTROO)
      return false;

   return true;
}

__forceinline bool BSPCanMoveInRoomTree3DInternal(const room_type* Room, SectorNode* SectorS, SectorNode* SectorE, Side* SideS, Side* SideE, const V2* Q)
{
   // block moves with end outside
   if (!SectorE || !SideE)
      return false;

   // sides which have no passable flag set always block
   if (SideS && !((SideS->Flags & WF_PASSABLE) == WF_PASSABLE))
      return false;

   // endsector must not be marked SF_NOMOVE
   if ((SectorE->Flags & SF_NOMOVE) == SF_NOMOVE)
      return false;

   // get floor and ceiling height at end
   const float hFloorE   = BSPGetSectorHeightFloorWithDepth(Room, SectorE, Q);
   const float hCeilingE = SECTORHEIGHTCEILING(SectorE, Q);

   // check endsector height
   if (hCeilingE - hFloorE < OBJECTHEIGHTROO)
      return false;

   // must evaluate heights at transition later
   unsigned int size = intersects.Size;
   if (size < MAXINTERSECTIONS)
   {
      intersects.Data[size].SectorS = SectorS;
      intersects.Data[size].SectorE = SectorE;
      intersects.Data[size].SideS = SideS;
      intersects.Data[size].SideE = SideE;
      intersects.Data[size].Q.X = Q->X;
      intersects.Data[size].Q.Y = Q->Y;
      intersects.Data[size].FloorHeight = (SectorS) ? BSPGetSectorHeightFloorWithDepth(Room, SectorS, Q) : 0.0f;
      intersects.Ptrs[size] = &intersects.Data[size];
      size++;
      intersects.Size = size;
   }
   else
      eprintf("BSPCanMoveInRoomTree3DInternal: Maximum trackable intersections reached!");

   return true;
}

__forceinline void BSPUpdateLeafHeights(const room_type* Room, const SectorNode* Sector, const bool Floor)
{
   // iterate all tree-nodes
   for (int i = 0; i < Room->TreeNodesCount; i++)
   {
      BspNode* node = &Room->TreeNodes[i];

      // skip non-leaf nodes, leafs with invalid sector or not the sector we're looking for
      if (node->Type != BspLeafType || !node->u.leaf.Sector || node->u.leaf.Sector != Sector)
         continue;

	  // iterate all points of this leaf
      for (int j = 0; j < node->u.leaf.PointsCount; j++)
      {
         V2 p = { node->u.leaf.PointsFloor[j].X, node->u.leaf.PointsFloor[j].Y };

         if (Floor)
            node->u.leaf.PointsFloor[j].Z = SECTORHEIGHTFLOOR(node->u.leaf.Sector, &p);

         else
            node->u.leaf.PointsCeiling[j].Z = SECTORHEIGHTCEILING(node->u.leaf.Sector, &p);
      }
   }
}

__forceinline bool BSPIntersectLineSplitter(const BspNode* Node, const V2* S, const V2* E, V2* P)
{
   // get 2d line equation coefficients for infinite line through S and E
   double a1, b1, c1;
   a1 = E->Y - S->Y;
   b1 = S->X - E->X;
   c1 = a1 * S->X + b1 * S->Y;

   // get 2d line equation coefficients of splitter
   double a2, b2, c2;
   a2 = -Node->u.internal.A;
   b2 = -Node->u.internal.B;
   c2 = Node->u.internal.C;

   const double det = a1*b2 - a2*b1;

   // if zero, they're parallel and never intersect
   if (ISZERO(det))
      return false;

   // intersection point of infinite lines
   P->X = (float)((b2*c1 - b1*c2) / det);
   P->Y = (float)((a1*c2 - a2*c1) / det);

   // must be in boundingbox of finite SE
   return ISINBOX(S, E, P);
}

bool BSPLineOfSightTree(const BspNode* Node, const V3* S, const V3* E)
{
   if (!Node)
      return true;

   /****************************************************************/

   // reached a leaf
   if (Node->Type == BspLeafType)
   {
      // no collisions with leafs without sectors
      if (!Node->u.leaf.Sector)
         return true;

      // floors and ceilings don't have backsides.
      // therefore a floor can only collide if
      // the start height is bigger than end height
      // and for ceiling the other way round.
      if (S->Z > E->Z && Node->u.leaf.Sector->FloorTexture > 0)
      {
         for (int i = 0; i < Node->u.leaf.PointsCount - 2; i++)
         {
            bool blocked = IntersectLineTriangle(
               &Node->u.leaf.PointsFloor[i + 2],
               &Node->u.leaf.PointsFloor[i + 1],
               &Node->u.leaf.PointsFloor[0], S, E);

            // blocked by floor
            if (blocked)
            {
#if DEBUGLOS
               dprintf("BLOCK - FLOOR");
#endif
               return false;
            }
         }
      }

      // check ceiling collision
      else if (S->Z < E->Z && Node->u.leaf.Sector->CeilingTexture > 0)
      {
         for (int i = 0; i < Node->u.leaf.PointsCount - 2; i++)
         {
            bool blocked = IntersectLineTriangle(
               &Node->u.leaf.PointsCeiling[i + 2],
               &Node->u.leaf.PointsCeiling[i + 1],
               &Node->u.leaf.PointsCeiling[0], S, E);

            // blocked by ceiling
            if (blocked)
            {
#if DEBUGLOS
               dprintf("BLOCK - CEILING sector %i", Node->u.leaf.SectorNum);
#endif
               return false;
            }
         }
      }

      // not blocked by this leaf
      return true;
   }

   /****************************************************************/

   // expecting anything else/below to be a splitter
   if (Node->Type != BspInternalType)
      return true;

   // get signed distances to both endpoints of ray
   const float distS = DISTANCETOSPLITTERSIGNED(&Node->u.internal, S);
   const float distE = DISTANCETOSPLITTERSIGNED(&Node->u.internal, E);

   /****************************************************************/

   // both endpoints on positive (right) side
   // --> climb down only right subtree
   if ((distS > EPSILON) & (distE > EPSILON))
      return BSPLineOfSightTree(Node->u.internal.RightChild, S, E);

   // both endpoints on negative (left) side
   // --> climb down only left subtree
   else if ((distS < -EPSILON) & (distE < -EPSILON))
      return BSPLineOfSightTree(Node->u.internal.LeftChild, S, E);

   // endpoints are on different sides or one/both on infinite line
   // --> check walls of splitter first and then possibly climb down both
   else
   {
      // intersect finite move-line SE with infinite splitter line
      // q stores possible intersection point
      V2 q;
      if (BSPIntersectLineSplitter(Node, (V2*)S, (V2*)E, &q))
      {
         // iterate finite segments (walls) in this splitter
         Wall* wall = Node->u.internal.FirstWall;
         while (wall)
         {
            // infinite intersection point must also be in bbox of wall
            // otherwise no intersect
            if (!ISINBOX(&wall->P1, &wall->P2, &q))
            {
               wall = wall->NextWallInPlane;
               continue;
            }

            // must have at least a sector on one side of the wall
            // otherwise skip this wall
            if (!wall->RightSector && !wall->LeftSector)
            {
               wall = wall->NextWallInPlane;
               continue;
            }

            // pick side ray is coming from
            Side* side = (distS > 0.0f) ? wall->RightSide : wall->LeftSide;

            // no collision with unset sides
            if (!side)
            {
               wall = wall->NextWallInPlane;
               continue;
            }

            // vector from (S)tart to (E)nd
            V3 se;
            V3SUB(&se, E, S);

            // find rayheight of (S->E) at intersection point
            float lambda = 1.0f;
            if (!ISZERO(se.X))
               lambda = (q.X - S->X) / se.X;

            else if (!ISZERO(se.Y))
               lambda = (q.Y - S->Y) / se.Y;

            float rayheight = S->Z + lambda * se.Z;

            // get heights of right and left floor/ceiling
            float hFloorRight = (wall->RightSector) ?
               SECTORHEIGHTFLOOR(wall->RightSector, &q) :
               SECTORHEIGHTFLOOR(wall->LeftSector, &q);

            float hFloorLeft = (wall->LeftSector) ?
               SECTORHEIGHTFLOOR(wall->LeftSector, &q) :
               SECTORHEIGHTFLOOR(wall->RightSector, &q);

            float hCeilingRight = (wall->RightSector) ?
               SECTORHEIGHTCEILING(wall->RightSector, &q) :
               SECTORHEIGHTCEILING(wall->LeftSector, &q);

            float hCeilingLeft = (wall->LeftSector) ?
               SECTORHEIGHTCEILING(wall->LeftSector, &q) :
               SECTORHEIGHTCEILING(wall->RightSector, &q);

            // build all 4 possible heights (h0 lowest)
            float h3 = MAX(hCeilingRight, hCeilingLeft);
            float h2 = MAX(MIN(hCeilingRight, hCeilingLeft), MAX(hFloorRight, hFloorLeft));
            float h1 = MIN(MIN(hCeilingRight, hCeilingLeft), MAX(hFloorRight, hFloorLeft));
            float h0 = MIN(hFloorRight, hFloorLeft);
			   
            // above maximum or below minimum
            if (rayheight > h3 || rayheight < h0)
            {
               wall = wall->NextWallInPlane;
               continue;
            }

            // ray intersects middle wall texture
            if (rayheight <= h2 && rayheight >= h1 && side->TextureMiddle > 0)
            {
               // get some flags from the side we're coming from
               // these are applied only to the 'main' = 'middle' texture
               bool isNoLookThrough = ((side->Flags & WF_NOLOOKTHROUGH) == WF_NOLOOKTHROUGH);
               bool isTransparent = ((side->Flags & WF_TRANSPARENT) == WF_TRANSPARENT);

               // 'transparent' middle textures block only
               // if they are set so by 'no-look-through'
               if (!isTransparent || (isTransparent && isNoLookThrough))
               {
#if DEBUGLOS
                  dprintf("WALL %i - MID - (%f/%f/%f)", wall->Num, q.X, q.Y, rayheight);
#endif
                  return false;
               }
            }

            // ray intersects upper wall texture
            if (rayheight <= h3 && rayheight >= h2 && side->TextureUpper > 0)
            {
#if DEBUGLOS
               dprintf("WALL %i - UP - (%f/%f/%f)", wall->Num, q.X, q.Y, rayheight);
#endif
               return false;
            }

            // ray intersects lower wall texture
            if (rayheight <= h1 && rayheight >= h0 && side->TextureLower > 0)
            {
#if DEBUGLOS
               dprintf("WALL %i - LOW - (%f/%f/%f)", wall->Num, q.X, q.Y, rayheight);
#endif
               return false;
            }

            // next wall for next loop
            wall = wall->NextWallInPlane;
         }
      }

      /****************************************************************/

      // try right subtree first
      bool retval = BSPLineOfSightTree(Node->u.internal.RightChild, S, E);

      // found a collision there? return it
      if (!retval)
         return retval;

      // otherwise try left subtree
      return BSPLineOfSightTree(Node->u.internal.LeftChild, S, E);
   }
}

template <bool USE3D, int MINDIST, int MINDIST2> bool BSPCanMoveInRoomTree(const room_type* Room, const BspNode* Node, const V2* S, const V2* E, Wall** BlockWall)
{
   // reached a leaf or nullchild, movements not blocked by leafs
   if (!Node || Node->Type != BspInternalType)
      return true;

   /****************************************************************/

   // get signed distances from splitter to both endpoints of move
   const float distS = DISTANCETOSPLITTERSIGNED(&Node->u.internal, S);
   const float distE = DISTANCETOSPLITTERSIGNED(&Node->u.internal, E);

   /****************************************************************/

   // both endpoints far away enough on positive (right) side
   // --> climb down only right subtree
   if ((distS > (float)MINDIST) & (distE > (float)MINDIST))
      return BSPCanMoveInRoomTree<USE3D, MINDIST, MINDIST2>(Room, Node->u.internal.RightChild, S, E, BlockWall);

   // both endpoints far away enough on negative (left) side
   // --> climb down only left subtree
   else if ((distS < (float)-MINDIST) & (distE < (float)-MINDIST))
      return BSPCanMoveInRoomTree<USE3D, MINDIST, MINDIST2>(Room, Node->u.internal.LeftChild, S, E, BlockWall);

   // endpoints are on different sides, or one/both on infinite line or potentially too close
   // --> check walls of splitter first and then possibly climb down both subtrees
   else
   {
      Side* sideS;
      SectorNode* sectorS;
      Side* sideE;
      SectorNode* sectorE;

      // CASE 1) The move line actually crosses this infinite splitter.
      // This case handles long movelines where S and E can be far away from each other and
      // just checking the distance of E to the line would fail.
      // q contains the intersection point
      if (((distS > 0.0f) && (distE < 0.0f)) ||
          ((distS < 0.0f) && (distE > 0.0f)))
      {
         // intersect finite move-line SE with infinite splitter line
         // q stores possible intersection point
         V2 q;
         if (BSPIntersectLineSplitter(Node, S, E, &q))
         {
            // iterate finite segments (walls) in this splitter
            Wall* wall = Node->u.internal.FirstWall;
            while (wall)
            {
               // OLD: infinite intersection point must also be in bbox of wall
               // otherwise no intersect
               //if (!ISINBOX(&wall->P1, &wall->P2, &q))
               // NEW: Check if the line of the wall intersects a circle consisting
               // of object's x, y and radius of min distance allowed to walls. Intersection
               // includes the wall being totally inside the circle. Must use WALLMINDISTANCE
               // else players can fit in areas smaller than player width.
               if (!IntersectOrInsideLineCircle(&q, (float)MINDIST, &wall->P1, &wall->P2))
               {
                  wall = wall->NextWallInPlane;
                  continue;
               }

               // set from and to sector / side
               if (distS > 0.0f)
               {
                  sideS = wall->RightSide;
                  sectorS = wall->RightSector;
               }
               else
               {
                  sideS = wall->LeftSide;
                  sectorS = wall->LeftSector;
               }

               if (distE > 0.0f)
               {
                  sideE = wall->RightSide;
                  sectorE = wall->RightSector;
               }
               else
               {
                  sideE = wall->LeftSide;
                  sectorE = wall->LeftSector;
               }

               // check the transition data for this wall, use intersection point q
               bool ok = (USE3D) ?
                  BSPCanMoveInRoomTree3DInternal(Room, sectorS, sectorE, sideS, sideE, &q) :
                  BSPCanMoveInRoomTreeInternal(Room, sectorS, sectorE, sideS, sideE, &q);

               if (!ok)
               {
                  *BlockWall = wall;
                  return false;
               }

               wall = wall->NextWallInPlane;
            }
         }		          
      }

      // CASE 2) The move line does not cross the infinite splitter, both move endpoints are on the same side.
      // This handles short moves where walls are not intersected, but the endpoint may be too close
      else
      {
         // check only getting closer
         if (fabs(distE) <= fabs(distS))
         {
            // iterate finite segments (walls) in this splitter
            Wall* wall = Node->u.internal.FirstWall;
            while (wall)
            {
               int useCase;

               // get min. squared distance from move endpoint to line segment
               const float dist2 = MinSquaredDistanceToLineSegment(E, &wall->P1, &wall->P2, &useCase);

               // skip if far enough away
               if (dist2 > (float)MINDIST2)
               {
                  wall = wall->NextWallInPlane;
                  continue;
               }

               // q stores closest point on line
               V2 q;
               if (useCase == 1)      q = wall->P1; // p1 is closest
               else if (useCase == 2) q = wall->P2; // p2 is closest
               else
               {
                  // line normal (90� vertical to line)
                  V2 normal;
                  normal.X = Node->u.internal.A;
                  normal.Y = Node->u.internal.B;

                  // flip normal if necessary (pick correct one of two)
                  if (distE > 0.0f)
                  {
                     V2SCALE(&normal, -1.0f);
                  }

                  V2SCALE(&normal, fabs(distE)); // set length of normal to distance to line
                  V2ADD(&q, E, &normal);         // q=E moved along the normal onto the line
               }

               // set from and to sector / side
               // for case 2 (too close) these are based on (S),
               // and (E) is assumed to be on the other side.
               if (distS >= 0.0f)
               {
                  sideS = wall->RightSide;
                  sectorS = wall->RightSector;
                  sideE = wall->LeftSide;
                  sectorE = wall->LeftSector;
               }
               else
               {
                  sideS = wall->LeftSide;
                  sectorS = wall->LeftSector;
                  sideE = wall->RightSide;
                  sectorE = wall->RightSector;
               }

               // check the transition data for this wall, use E for intersectpoint
               bool ok = (USE3D) ?
                  BSPCanMoveInRoomTree3DInternal(Room, sectorS, sectorE, sideS, sideE, &q) :
                  BSPCanMoveInRoomTreeInternal(Room, sectorS, sectorE, sideS, sideE, &q);

               if (!ok)
               {
                  *BlockWall = wall;
                  return false;
               }

               wall = wall->NextWallInPlane;
            }
         }
      }

      /****************************************************************/

      // try right subtree first
      bool retval = BSPCanMoveInRoomTree<USE3D, MINDIST, MINDIST2>(Room, Node->u.internal.RightChild, S, E, BlockWall);

      // found a collision there? return it
      if (!retval)
         return retval;

      // otherwise try left subtree
      return BSPCanMoveInRoomTree<USE3D, MINDIST, MINDIST2>(Room, Node->u.internal.LeftChild, S, E, BlockWall);
   }
}
#pragma endregion

#pragma region Public
/**************************************************************************************************************/
/*                                            PUBLIC                                                          */
/*                     These are defined in header and can be called from outside                             */
/**************************************************************************************************************/

/*********************************************************************************************/
/* BSPGetHeight:  Returns true if location is inside any sector, false otherwise.
/*                  If true, heights are in parameters HeightF (floor), 
/*				    HeightFWD (floor with depth) and HeightC (ceiling) and Leaf is valid.
/*********************************************************************************************/
bool BSPGetHeight(room_type* Room, V2* P, float* HeightF, float* HeightFWD, float* HeightC, BspLeaf** Leaf)
{
   // sanity check: these are binary or operations because it's very unlikely
   // any condition is met. So next ones can't be skipped, so binary is faster.
   if (!Room || ((Room->TreeNodesCount == 0) | !P | !HeightF | !HeightFWD | !HeightC))
      return false;

   // start with root-node
   BspNode* n = &Room->TreeNodes[0];

   // traverse tree
   while (n)
   {
      // still processing a splitter (tree-node)
      if (n->Type == BspInternalType)
      {
         // see whether to climb down left or right sub-tree
         const float dist = DISTANCETOSPLITTERSIGNED(&n->u.internal, P);

         // pick child and loop again
         n = (dist >= 0.0f) ? n->u.internal.RightChild : n->u.internal.LeftChild;
      }
      
      // reached a subsector (tree-leaf)
      else if (n->Type == BspLeafType && n->u.leaf.Sector)
      {
         // set output params
         *Leaf = &n->u.leaf;
         *HeightF = SECTORHEIGHTFLOOR(n->u.leaf.Sector, P);
         *HeightFWD = BSPGetSectorHeightFloorWithDepth(Room, n->u.leaf.Sector, P);
         *HeightC = SECTORHEIGHTCEILING(n->u.leaf.Sector, P);
         return true;
      }

      // unknown nodetype, wtf?
      // need to prevent infin. loop here
      else
         return false;
   }

   // outside of any leaf
   return false;
}

/*********************************************************************************************/
/* BSPLineOfSightView: Checks if (player) with coordinates S can see target E, if player is     */
/*                  facing kod_angle direction (max angle 4096, 0 east, increase clockwise.  */
/*********************************************************************************************/
bool BSPLineOfSightView(V2 *S, V2 *E, int kod_angle)
{
   float radangle = KODANGLETORADIANS(kod_angle);
   E->X -= S->X;
   E->Y -= S->Y;

   // Check behind player.
   float center_a = cosf(radangle) * ROOFINENESS;
   float center_b = sinf(radangle) * ROOFINENESS;
   if (center_a * E->X + center_b * E->Y < 0)
   {
#if DEBUGLOSVIEW
      dprintf("BSPLineOfSightView block - target behind player.\n");
#endif
      return false;
   }

   // Check to left of player's view frustum.
   float left_a = -center_b + ((center_a * HALFFRUSTUMWIDTH) / HALFROOFINENESS);
   float left_b = center_a + ((center_b * HALFFRUSTUMWIDTH) / HALFROOFINENESS);
   if (left_a * E->X + left_b * E->Y < 0)
   {
#if DEBUGLOSVIEW
      dprintf("BSPLineOfSightView block - target to left of player.\n");
#endif
      return false;
   }

   // Check to right of player's view frustum.
   float right_a = center_b + ((center_a * HALFFRUSTUMWIDTH) / HALFROOFINENESS);
   float right_b = -center_a + ((center_b * HALFFRUSTUMWIDTH) / HALFROOFINENESS);
   if (right_a * E->X + right_b * E->Y < 0)
   {
#if DEBUGLOSVIEW
      dprintf("BSPLineOfSightView block - target to right of player.\n");
#endif
      return false;
   }

   return true;
}

/*********************************************************************************************/
/* BSPLineOfSight:  Checks if location E(nd) can be seen from location S(tart)               */
/*********************************************************************************************/
bool BSPLineOfSight(room_type* Room, V3* S, V3* E)
{
   // sanity check: these are binary or operations because it's very unlikely
   // any condition is met. So next ones can't be skipped, so binary is faster.
   if (!Room || ((Room->TreeNodesCount == 0) | !S | !E))
      return false;

   // test center
   if (BSPLineOfSightTree(&Room->TreeNodes[0], S, E))
      return true;

   V3 e;

   // test lower
   e.X = E->X;
   e.Y = E->Y;
   e.Z = E->Z - OBJECTHEIGHTROO + 1;
   if (BSPLineOfSightTree(&Room->TreeNodes[0], S, &e))
      return true;

   // test p
   e.X = E->X + LOSEXTEND;
   e.Y = E->Y + LOSEXTEND;
   e.Z = E->Z;
   if (BSPLineOfSightTree(&Room->TreeNodes[0], S, &e))
      return true;

   // test p
   e.X = E->X - LOSEXTEND;
   e.Y = E->Y - LOSEXTEND;
   e.Z = E->Z;
   if (BSPLineOfSightTree(&Room->TreeNodes[0], S, &e))
      return true;

   // test p
   e.X = E->X + LOSEXTEND;
   e.Y = E->Y - LOSEXTEND;
   e.Z = E->Z;
   if (BSPLineOfSightTree(&Room->TreeNodes[0], S, &e))
      return true;

   // test p
   e.X = E->X - LOSEXTEND;
   e.Y = E->Y + LOSEXTEND;
   e.Z = E->Z;
   if (BSPLineOfSightTree(&Room->TreeNodes[0], S, &e))
      return true;

   return false;
}

/*********************************************************************************************/
/* BSPCanMoveInRoom:  Checks if you can walk a straight line from (S)tart to (E)nd           */
/*                    This is a 2D variant which assumes all wall transitions are made on    */
/*                    ground level. For better variant see 3D. Still used in Astar           */
/*********************************************************************************************/
bool BSPCanMoveInRoom(room_type* Room, V2* S, V2* E, int ObjectID, bool moveOutsideBSP, bool ignoreBlockers, bool ignoreEndBlocker, Wall** BlockWall)
{
   // sanity check: these are binary or operations because it's very unlikely
   // any condition is met. So next ones can't be skipped, so binary is faster.
   if (!Room || ((Room->TreeNodesCount == 0) | !S | !E))
      return false;

   // allow move to same location
   if (ISZERO(S->X - E->X) && ISZERO(S->Y - E->Y))
   {
#if DEBUGMOVE
      dprintf("MOVEALLOW (START=END)");
#endif
      return true;
   }

   // first check against room geometry
   bool roomok = (moveOutsideBSP || BSPCanMoveInRoomTree<false, (int)WALLMINDISTANCE, (int)WALLMINDISTANCE2>(Room, &Room->TreeNodes[0], S, E, BlockWall));

   // already found a collision in room
   if (!roomok)
      return false;

   // don't validate objects, we're done
   if (ignoreBlockers)
      return true;

   // otherwise also check against blockers
   BlockerNode* blocker = Room->Blocker;
   while (blocker)
   {
      // don't block ourself
      if (blocker->ObjectID == ObjectID)
      {
         blocker = blocker->Next;
         continue;
      }

      // don't block by object at end
      if (ignoreEndBlocker)
      {
         V2 db; // from blocker to end
         V2SUB(&db, &blocker->Position, E);
         const float db2 = V2LEN2(&db);
         if (db2 <= EPSILON)
         {
            blocker = blocker->Next;
            continue;
         }
      }

      V2 ms; // from m to s  
      V2SUB(&ms, S, &blocker->Position);
      float ds2 = V2LEN2(&ms);

      // CASE 1) Start is too close
      // Note: IntersectLineCircle below will reject moves starting or ending exactly
      //   on the circle as well as moves going from inside to outside of the circle.
      //   So this case here must handle moves until the object is out of radius again.
      if (ds2 <= OBJMINDISTANCE2)
      {
         V2 me;
         V2SUB(&me, E, &blocker->Position); // from m to e
         float de2 = V2LEN2(&me);

         // end must be farer away than start
         if (de2 <= ds2)
            return false;
      }

      // CASE 2) Start is outside blockradius, verify by intersection algorithm.
      else
      {
         if (IntersectLineCircle(&blocker->Position, OBJMINDISTANCE, S, E))
         {
#if DEBUGMOVE
            dprintf("MOVEBLOCK BY OBJ %i",blocker->ObjectID);
#endif
            return false;
         }
      }
      blocker = blocker->Next;
   }

   return true;
}

/*********************************************************************************************/
/* BSPCanMoveInRoom3D:  Works like 2D variant but supports dynamic height of objects along   */
/*                      the move-line. Hence jumps are supported here.                       */
/*********************************************************************************************/
template<bool ISPLAYER> bool BSPCanMoveInRoom3D(room_type* Room, V2* S, V2* E, float Height, float Speed, int ObjectID, bool moveOutsideBSP, bool ignoreBlockers, bool ignoreEndBlocker, Wall** BlockWall)
{
   // sanity check: these are binary or operations because it's very unlikely
   // any condition is met. So next ones can't be skipped, so binary is faster.
   if (!Room || ((Room->TreeNodesCount == 0) | !S | !E))
      return false;

   // allow move to same location
   if (ISZERO(S->X - E->X) && ISZERO(S->Y - E->Y))
   {
#if DEBUGMOVE
      dprintf("MOVEALLOW (START=END)");
#endif
      return true;
   }

   // first check against room geometry
   if (!moveOutsideBSP)
   {
      // reset intermediate intersections results
      intersects.Size = 0;

      // basic roomtest, might return false already for a block
      // template branching for user/monsters
      if (ISPLAYER)
      {
         if (!BSPCanMoveInRoomTree<true, (int)WALLMINDISTANCEUSER, (int)WALLMINDISTANCEUSER2>(Room, &Room->TreeNodes[0], S, E, BlockWall))
            return false;
      }
      else
      {
         // basic roomtest, might return false already for a block
         if (!BSPCanMoveInRoomTree<true, (int)WALLMINDISTANCE, (int)WALLMINDISTANCE2>(Room, &Room->TreeNodes[0], S, E, BlockWall))
            return false;
      }

      // but still got to validate height transitions list
      // calculate the squared distances
      for (unsigned int i = 0; i < intersects.Size; i++)
      {
         V2 v;
         V2SUB(&v, &intersects.Data[i].Q, S);
         intersects.Data[i].Distance2 = V2LEN2(&v);
      }

      // sort the potential intersections by squared distance from start
      BSPIntersectionsSort(intersects.Ptrs, 0, intersects.Size);

      // debug
      /*if (intersects.Size > 2)
      {
         //dprintf("%f %f %f", intersects.Ptrs[0]->Distance, intersects.Ptrs[1]->Distance, intersects.Ptrs[2]->Distance);
         dprintf("%i", intersects.Size);
         dprintf("-----------------------------");
      }*/

      // iterate from intersection to intersection, starting and start and ending at end
      // check the transition data for each one, use intersection point q
      float distanceDone = 0.0f;
      float heightModified = Height;
      V2* p = S;
      for (unsigned int i = 0; i < intersects.Size; i++)
      {
         Intersection* transit = intersects.Ptrs[i];

         // pick max stepheight based on player/monster
         // this gives some tolerance for users, but clients should use MAXSTEPHEIGHT
         const float MAXSTEPH = (ISPLAYER) ? MAXSTEPHEIGHTUSER : MAXSTEPHEIGHT;

         // deal with null start sector/side
         if (!transit->SideS || !transit->SectorS)
         {
            if (i == 0) break; // allow if first segment (start outside)
            else return false; // deny otherwise (segment on the line outside)
         }

         // get floor heights
         float hFloorSP = BSPGetSectorHeightFloorWithDepth(Room, transit->SectorS, p);
         float hFloorSQ = BSPGetSectorHeightFloorWithDepth(Room, transit->SectorS, &transit->Q);
         float hFloorEQ = BSPGetSectorHeightFloorWithDepth(Room, transit->SectorE, &transit->Q);

         // squared length of this segment
         float stepLen2 = transit->Distance2 - distanceDone;

         // reduce height by what we lose across the SectorS from p to q
         if (heightModified > hFloorSP)
         {
            // workaround div by 0, todo? these are teleports
            if (Speed < 0.001f && Speed > -0.001f) Speed = 9999999;

            // S = 1/2*a*t� where t=dist/speed
            // S = 1/2*a*(dist�/speed�)
            // t� = (dist/speed)� = dist�/speed�
            float stepDt2 = stepLen2 / (Speed*Speed);
            float stepFall = 0.5f * GRAVITYACCELERATION * stepDt2;

            // apply fallheight
            heightModified += stepFall;
         }

         // too far below sector
         else if (heightModified < (hFloorSP - MAXSTEPH))
            return false;

         // make sure we're at least at startsector's groundheight at Q when we reach Q from P
         // in case we stepped up or fell below it
         heightModified = MAX(hFloorSQ, heightModified);
         
         // check stepheight (this also requires a lower texture set)
         //if (transit->SideS->TextureLower > 0 && (hFloorE - hFloorQ > MAXSTEPHEIGHT))
         if (transit->SideS->TextureLower > 0 && (hFloorEQ - heightModified > MAXSTEPH))
            return false;

         // get ceiling height
         const float hCeilingEQ = SECTORHEIGHTCEILING(transit->SectorE, &transit->Q);

         // check ceilingheight (this also requires an upper texture set)
         if (transit->SideS->TextureUpper > 0 && (hCeilingEQ - hFloorSQ < OBJECTHEIGHTROO))
            return false;

         // we actually made it across that intersection
         heightModified = MAX(hFloorEQ, heightModified); // keep our height or set it at least to sector
         distanceDone += stepLen2;                       // add squared length of processed segment
         p = &transit->Q;                                // set end to next start
      }
   }

   // don't validate objects, we're done
   if (ignoreBlockers)
      return true;

   // get current ms tick
   const unsigned int tick = (unsigned int)GetMilliCount();

   // otherwise also check against blockers
   BlockerNode* blocker = Room->Blocker;
   while (blocker)
   {
      // ignore this blocker if we're a player
      // and it has moved too recently
      if (ISPLAYER && (tick - blocker->TickLastMove < NOBLOCKOBJUSERDELAY))
      {
         blocker = blocker->Next;
         continue;
      }

      // don't block ourself
      if (blocker->ObjectID == ObjectID)
      {
         blocker = blocker->Next;
         continue;
      }

      // don't block by object at end
      if (ignoreEndBlocker)
      {
         V2 db; // from blocker to end
         V2SUB(&db, &blocker->Position, E);
         const float db2 = V2LEN2(&db);
         if (db2 <= EPSILON)
         {
            blocker = blocker->Next;
            continue;
         }
      }

      V2 ms; // from m to s  
      V2SUB(&ms, S, &blocker->Position);
      float ds2 = V2LEN2(&ms);

      // CASE 1) Start is too close
      // Note: IntersectLineCircle below will reject moves starting or ending exactly
      //   on the circle as well as moves going from inside to outside of the circle.
      //   So this case here must handle moves until the object is out of radius again.
      if (ds2 <= ((ISPLAYER) ? OBJMINDISTANCEUSER2 : OBJMINDISTANCE2))
      {
         V2 me;
         V2SUB(&me, E, &blocker->Position); // from m to e
         float de2 = V2LEN2(&me);

         // end must be farer away than start
         if (de2 <= ds2)
            return false;
      }

      // CASE 2) Start is outside blockradius, verify by intersection algorithm.
      else
      {
         if (IntersectLineCircle(&blocker->Position, (ISPLAYER) ? OBJMINDISTANCEUSER : OBJMINDISTANCE, S, E))
         {
#if DEBUGMOVE
            dprintf("MOVEBLOCK BY OBJ %i", blocker->ObjectID);
#endif
            return false;
         }
      }
      blocker = blocker->Next;
   }

   return true;
}
template bool BSPCanMoveInRoom3D<true>(room_type* Room, V2* S, V2* E, float Height, float Speed, int ObjectID, bool moveOutsideBSP, bool ignoreBlockers, bool ignoreEndBlocker, Wall** BlockWall);
template bool BSPCanMoveInRoom3D<false>(room_type* Room, V2* S, V2* E, float Height, float Speed, int ObjectID, bool moveOutsideBSP, bool ignoreBlockers, bool ignoreEndBlocker, Wall** BlockWall);

/*********************************************************************************************/
/* BSPChangeTexture: Sets textures of sides and/or sectors to given NewTexture num based on Flags
/*********************************************************************************************/
void BSPChangeTexture(room_type* Room, unsigned int ServerID, unsigned short NewTexture, unsigned int Flags)
{
   bool isAboveWall  = ((Flags & CTF_ABOVEWALL) == CTF_ABOVEWALL);
   bool isNormalWall = ((Flags & CTF_NORMALWALL) == CTF_NORMALWALL);
   bool isBelowWall  = ((Flags & CTF_BELOWWALL) == CTF_BELOWWALL);
   bool isFloor      = ((Flags & CTF_FLOOR) == CTF_FLOOR);
   bool isCeiling    = ((Flags & CTF_CEILING) == CTF_CEILING);
   bool isReset      = ((Flags & CTF_RESET) == CTF_RESET);

   // change on sides
   if (isAboveWall || isNormalWall || isBelowWall)
   {
      for (int i = 0; i < Room->SidesCount; i++)
      {
         Side* side = &Room->Sides[i];

         // server ID does not match
         if (side->ServerID != ServerID)
            continue;

         if (isAboveWall)
            side->TextureUpper = (isReset ? side->TextureUpperOrig : NewTexture);

         if (isNormalWall)
            side->TextureMiddle = (isReset ? side->TextureMiddleOrig : NewTexture);

         if (isBelowWall)
            side->TextureLower = (isReset ? side->TextureLowerOrig : NewTexture);
      }
   }

   // change on sectors
   if (isFloor || isCeiling)
   {
      for (int i = 0; i < Room->SectorsCount; i++)
      {
         SectorNode* sector = &Room->Sectors[i];

         // server ID does not match
         if (sector->ServerID != ServerID)
            continue;

         if (isFloor)
            sector->FloorTexture = (isReset ? sector->FloorTextureOrig : NewTexture);

         if (isCeiling)
            sector->CeilingTexture = (isReset ? sector->CeilingTextureOrig : NewTexture);
      }
   }

   AStarClearEdgesCache(Room);
   AStarClearPathCaches(Room);
}

/*********************************************************************************************/
/* BSPChangeSectorFlag: Sets or resets sector flags.
/*********************************************************************************************/
void BSPChangeSectorFlag(room_type* Room, unsigned int ServerID, unsigned int ChangeFlag)
{
   bool isReset = ((ChangeFlag & CSF_RESET_ALL) == CSF_RESET_ALL);
   bool depthReset = ((ChangeFlag & CSF_DEPTHMASK) == CSF_DEPTH_RESET);
   bool noMoveReset = ((ChangeFlag & CSF_NOMOVEMASK) == CSF_NOMOVE_RESET);
   bool noMoveOn = ((ChangeFlag & CSF_NOMOVEMASK) == CSF_NOMOVE_ON);
   bool noMoveOff = ((ChangeFlag & CSF_NOMOVEMASK) == CSF_NOMOVE_OFF);
   short depth = -1;
   if ((ChangeFlag & CSF_DEPTHMASK) == CSF_DEPTH0)
      depth = SF_DEPTH0;
   else if ((ChangeFlag & CSF_DEPTHMASK) == CSF_DEPTH1)
      depth = SF_DEPTH1;
   else if ((ChangeFlag & CSF_DEPTHMASK) == CSF_DEPTH2)
      depth = SF_DEPTH2;
   else if ((ChangeFlag & CSF_DEPTHMASK) == CSF_DEPTH3)
      depth = SF_DEPTH3;

   for (int i = 0; i < Room->SectorsCount; i++)
   {
      SectorNode* sector = &Room->Sectors[i];

      // server ID does not match
      if (sector->ServerID != ServerID)
         continue;

      // Reset all flags to original values.
      if (isReset)
      {
         sector->Flags = sector->FlagsOrig;
         continue;
      }

      // Reset to original depth.
      if (depthReset)
      {
         sector->Flags &= ~SF_MASK_DEPTH;
         sector->Flags |= (sector->FlagsOrig & SF_MASK_DEPTH);
      }
      // Set a new depth.
      else if (depth >= 0)
      {
         sector->Flags &= ~SF_MASK_DEPTH;
         sector->Flags |= depth;
      }

      // Reset nomove status.
      if (noMoveReset)
      {
         if ((sector->FlagsOrig & SF_NOMOVE) == SF_NOMOVE)
            sector->Flags |= SF_NOMOVE;
         else
            sector->Flags &= ~SF_NOMOVE;
      }
      // Set a new nomove status.
      else if (noMoveOn)
         sector->Flags |= SF_NOMOVE;
      else if (noMoveOff)
         sector->Flags &= ~SF_NOMOVE;
   }

   AStarClearEdgesCache(Room);
   AStarClearPathCaches(Room);
}

/*********************************************************************************************/
/* BSPMoveSector:  Adjust floor or ceiling height of a non-sloped sector. 
/*                 Always instant for now. Otherwise only for speed=0. Height must be in 1:1024.
/*********************************************************************************************/
void BSPMoveSector(room_type* Room, unsigned int ServerID, bool Floor, float Height, float Speed)
{
   for (int i = 0; i < Room->SectorsCount; i++)
   {
      SectorNode* sector = &Room->Sectors[i];

      // server ID does not match
      if (sector->ServerID != ServerID)
         continue;

      // move floor
      if (Floor)
      {
         sector->FloorHeight = Height;
         BSPUpdateLeafHeights(Room, sector, true);
      }

      // move ceiling
      else
      {
         sector->CeilingHeight = Height;
         BSPUpdateLeafHeights(Room, sector, false);
      }
   }

   AStarClearEdgesCache(Room);
   AStarClearPathCaches(Room);
}

/*********************************************************************************************/
/* BSPGetSectorHeight:  Returns the floor or ceiling height of the first sector matching ID
/*********************************************************************************************/
bool BSPGetSectorHeightByID(room_type* Room, unsigned int ServerID, bool Floor, float* Height)
{
   for (int i = 0; i < Room->SectorsCount; i++)
   {
      SectorNode* sector = &Room->Sectors[i];

      // server ID match
      if (sector->ServerID == ServerID)
      {
         *Height = (Floor) ? sector->FloorHeight : sector->CeilingHeight;
         return true;
      }
   }

   return false;
}

/*********************************************************************************************/
/* BSPGetLocationInfo:  Returns several infos about a location depending on 'QueryFlags'
/*********************************************************************************************/
bool BSPGetLocationInfo(room_type* Room, V2* P, unsigned int QueryFlags, unsigned int* ReturnFlags,
                        float* HeightF, float* HeightFWD, float* HeightC, BspLeaf** Leaf)
{
   // sanity check: these are binary or operations because it's very unlikely
   // any condition is met. So next ones can't be skipped, so binary is faster.
   if (!Room | !P | !ReturnFlags)
      return false;

   // see what to query
   bool isCheckThingsBox = ((QueryFlags & LIQ_CHECK_THINGSBOX) == LIQ_CHECK_THINGSBOX);
   bool isCheckOjectBlock = ((QueryFlags & LIQ_CHECK_OBJECTBLOCK) == LIQ_CHECK_OBJECTBLOCK);
   bool isGetSectorInfo = ((QueryFlags & LIQ_GET_SECTORINFO) == LIQ_GET_SECTORINFO);
   
   // check if output parameters are provided if query-type needs them
   if (isGetSectorInfo && (!HeightF || !HeightFWD || !HeightC || !Leaf))
      return false;

   // default returnflags
   *ReturnFlags = 0;

   // check outside thingsbox
   if (isCheckThingsBox)
   {
      // out west
      if (P->X <= Room->ThingsBox.Min.X)
         *ReturnFlags |= LIR_TBOX_OUT_W;

      // out east
      else if (P->X >= Room->ThingsBox.Max.X)
         *ReturnFlags |= LIR_TBOX_OUT_E;

      // out north
      if (P->Y <= Room->ThingsBox.Min.Y)
         *ReturnFlags |= LIR_TBOX_OUT_N;

      // out south
      else if (P->Y >= Room->ThingsBox.Max.Y)
         *ReturnFlags |= LIR_TBOX_OUT_S;
   }

   // check too close to blocker
   if (isCheckOjectBlock)
   {
      BlockerNode* blocker = Room->Blocker;
      while (blocker)
      {
         V2 b;
         V2SUB(&b, P, &blocker->Position);

         // too close
         if (V2LEN2(&b) < OBJMINDISTANCE2)
         {
            *ReturnFlags |= LIR_BLOCKED_OBJECT;
            break;
         }
         blocker = blocker->Next;
      }
   }

   // bsp lookup
   if (isGetSectorInfo && BSPGetHeight(Room, P, HeightF, HeightFWD, HeightC, Leaf))
   {
      *ReturnFlags |= LIR_SECTOR_INSIDE;

      if ((*Leaf)->Sector->FloorTexture > 0)
         *ReturnFlags |= LIR_SECTOR_HASFTEX;

      if ((*Leaf)->Sector->CeilingTexture > 0)
         *ReturnFlags |= LIR_SECTOR_HASCTEX;

      if (((*Leaf)->Sector->Flags & SF_NOMOVE) == SF_NOMOVE)
         *ReturnFlags |= LIR_SECTOR_NOMOVE;

      const unsigned int depthtype = (*Leaf)->Sector->Flags & SF_MASK_DEPTH;

      if (depthtype == SF_DEPTH0)       *ReturnFlags |= LIR_SECTOR_DEPTH0;
      else if (depthtype == SF_DEPTH1)  *ReturnFlags |= LIR_SECTOR_DEPTH1;
      else if (depthtype == SF_DEPTH2)  *ReturnFlags |= LIR_SECTOR_DEPTH2;
      else if (depthtype == SF_DEPTH3)  *ReturnFlags |= LIR_SECTOR_DEPTH3;
   }

   return true;
}

/*********************************************************************************************/
/* BSPGetRandomPoint: Tries up to 'MaxAttempts' times to create a randompoint in 'Room'.
/*                    If return is true, P's coordinates are guaranteed to be:
/*                    (a) inside a sector               (b) inside thingsbox 
/*                    (c) outside any obj blockradius   (d) not on sector marked SF_NOMOVE
/*                    (e) capable of a step in each direction with 'UnblockedRadius' length
/*********************************************************************************************/
bool BSPGetRandomPoint(room_type* Room, int MaxAttempts, float UnblockedRadius, V2* P)
{
   // sanity check: these are binary or operations because it's very unlikely
   // any condition is met. So next ones can't be skipped, so binary is faster.
   if (!Room | !P)
      return false;

   float heightF, heightFWD, heightC;
   BspLeaf* leaf = NULL;

   for (int i = 0; i < MaxAttempts; i++)
   {
      // generate random coordinates inside the things box
      // we first map the random value to [0.0f , 1.0f] and then to [0.0f , BBOXMAX]
      // note: the minimum of thingsbox is always at 0/0
      P->X = ((float)rand() / (float)RAND_MAX) * Room->ThingsBox.Max.X;
      P->Y = ((float)rand() / (float)RAND_MAX) * Room->ThingsBox.Max.Y;

      // make sure point is exactly expressable in KOD fineness units also
      V2ROUNDROOTOKODFINENESS(P);

      // 1. check for inside valid sector, otherwise roll again
      // note: locations quite close to a wall pass this check!
      if (!BSPGetHeight(Room, P, &heightF, &heightFWD, &heightC, &leaf))
         continue;

      // 2. must also have floor texture set and not marked as SF_NOMOVE, otherwise roll again
      if (leaf && (leaf->Sector->FloorTexture == 0 || ((leaf->Sector->Flags & SF_NOMOVE) == SF_NOMOVE)))
         continue;

      // 3. make sure we can step in all directions
      // prevents points too close to walls and object collisions
      Wall* wall;
      V2 v, stepend;

      // try rotating test-vector in 8 steps of 45� up to 360�
      if (UnblockedRadius >= 1.0f)
      {
         v.X = UnblockedRadius;
         v.Y = 0;
         bool radiuscheck = true;
         for (int j = 0; j < 8; j++)
         {
            V2ROTATE(&v, 0.25f * (float)-M_PI);
            V2ADD(&stepend, P, &v);
            V2ROUNDROOTOKODFINENESS(&stepend);
            if (!BSPCanMoveInRoom(Room, P, &stepend, 0, false, false, false, &wall))
            {
               radiuscheck = false;
               break;
            }
         }

         // radiuscheck failed
         if (!radiuscheck)
            continue;
      }

      // 4. check for being too close to a blocker
      BlockerNode* blocker = Room->Blocker;
      bool collision = false;
      while (blocker)
      {
         V2 b;
         V2SUB(&b, P, &blocker->Position);

         // too close
         if (V2LEN2(&b) < OBJMINDISTANCE2)
         {
            collision = true;
            break;
         }

         blocker = blocker->Next;
      }

      // too close to a blocker, roll again
      if (collision)
         continue;

      // all good with P
      return true;
   }

   // max attempts reached without success
   return false;
}

/*********************************************************************************************/
/* BSPGetRandomMoveDest:
/*  Tries up to 'MaxAttempts' times to create a random move destination in 'Room'.
/*  The point has the same guarantees as the return of BSPGetRandomPoint(), but additionally is
/*  also guaranteed to be within a limited distance from P and the height diff of S and P is limited.
/*********************************************************************************************/
bool BSPGetRandomMoveDest(room_type* Room, int MaxAttempts, float MinDistance, float MaxDistance, V2* S, V2* P)
{
   if (!Room | !S | !P)
      return false;

   ///////////////////////////////////////////////////////////////////////////////////////////////
   // GET START HEIGHT/SECTOR

   float heightSF, heightSFWD, heightSC;
   BspLeaf* leafS = NULL;

   // check start inside valid sector, get start sector and location heights
   if (!BSPGetHeight(Room, S, &heightSF, &heightSFWD, &heightSC, &leafS) || !leafS)
      return false;

   ///////////////////////////////////////////////////////////////////////////////////////////////
   // FIND MOVE DESTINATION POINT

   // difference between maximum and minimum distance
   const float distDelta = MaxDistance - MinDistance;
   
   float heightF, heightFWD, heightC;
   BspLeaf* leaf = NULL;

   for (int i = 0; i < MaxAttempts; i++)
   {
      // generate a random vector length and rotation
      const float length = MinDistance + (((float)rand() / (float)RAND_MAX) * distDelta);
      const float rotate = ((float)rand() / (float)RAND_MAX) * PI_MULT_2;

      // unit X vector
      V2 v; v.X = 1.0f; v.Y = 0.0f;

      // rotate and scale by generated randoms
      V2ROTATE(&v, rotate);
      V2SCALE(&v, length);

      // then add up on start to generate point
      V2ADD(P, S, &v);

      // make sure point is exactly expressable in KOD fineness units also
      V2ROUNDROOTOKODFINENESS(P);

      ////// CHECKS ///////

      // [1] check for inside things box, otherwise roll again
      if (!ISINBOX(&Room->ThingsBox.Min, &Room->ThingsBox.Max, P))
         continue;

      // [2] check random point is inside valid sector, otherwise roll again
      // note: locations quite close to a wall pass this check!
      if (!BSPGetHeight(Room, P, &heightF, &heightFWD, &heightC, &leaf) || !leaf)
         continue;

      // [3] must also have floor texture set and not marked as SF_NOMOVE, otherwise roll again
      if ((leaf->Sector->FloorTexture == 0) || ((leaf->Sector->Flags & SF_NOMOVE) == SF_NOMOVE))
         continue;

      // get height difference
      const float delta  = heightFWD - heightSFWD;
      const float delta2 = delta * delta;

      // [4] if height of destination is much higher or lower, roll again
      if (delta2 > MAXRNDMOVEDELTAH2)
         continue;

      // [5] check for destination being too close to a blocker
      BlockerNode* blocker = Room->Blocker;
      bool collision = false;
      while (blocker)
      {
         V2 b;
         V2SUB(&b, P, &blocker->Position);

         // too close
         if (V2LEN2(&b) < OBJMINDISTANCE2)
         {
            collision = true;
            break;
         }

         blocker = blocker->Next;
      }

      // too close to a blocker, roll again
      if (collision)
         continue;

      // all good with P
      return true;
   }

   // failed to generate point
   return false;
}

/*********************************************************************************************/
/* BSPGetStepTowards:  Returns a location in P param, in a distant of 16 kod fineness units
/*                     away from S on the way towards E.
/*********************************************************************************************/
bool BSPGetStepTowards(room_type* Room, V2* S, V2* E, V2* P, unsigned int* Flags, int ObjectID, float Speed, float Height)
{
   // sanity check: these are binary or operations because it's very unlikely
   // any condition is met. So next ones can't be skipped, so binary is faster.
   if (!Room | !S | !E | !P | !Flags)
      return false;

   // Monsters that can move through walls or outside the tree will
   // send this flag with state.
   bool moveOutsideBSP = ((*Flags & MSTATE_MOVE_OUTSIDE_BSP) == MSTATE_MOVE_OUTSIDE_BSP);

   // unset several flags:
   *Flags &= ~MSTATE_MOVE_OUTSIDE_BSP; // must not give these back in piState (was hijacked by caller)
   *Flags &= ~ESTATE_LONG_STEP;        // unset possible longstep flag from last astar step
   *Flags &= ~STATE_MOVE_PATH;         // unset possible pathmoving flag
   *Flags &= ~STATE_MOVE_DIRECT;       // unset possible directmoving flag

   V2 se, stepend;
   V2SUB(&se, E, S);

   // get length from start to end
   float len = V2LEN(&se);

   // trying to step to old location?
   if (ISZERO(len))
   {
      // set step destination to end
      *P = *E;
      *Flags &= ~ESTATE_AVOIDING;
      *Flags &= ~ESTATE_CLOCKWISE;
      return true;
   }

   // this first normalizes the se vector,
   // then scales to a length of 16 kod-fineunits (=256  roo-fineunits)
   float scale = (1.0f / len) * FINENESSKODTOROO(16.0f);

   // apply the scale on se
   V2SCALE(&se, scale);

   /****************************************************/
   // 1) test direct line towards destination first
   /****************************************************/
   Wall* blockWall = NULL;

   // we do not care about object blocks here, since this is no real step
   if (BSPCanMoveInRoom3D<false>(Room, S, E, Height, Speed, ObjectID, moveOutsideBSP, false, true, &blockWall))
   {
      // note: we must verify the location the object is actually going to end up in KOD,
      // this means we must round to the next closer kod-fineness value,  
      // so these values are also exactly expressable in kod coordinates.
      // in fact this makes the vector a variable length between ~15.5 and ~16.5 fine units
      // this time object blocks matter
      V2ADD(&stepend, S, &se);
      V2ROUNDROOTOKODFINENESS(&stepend);
      if (BSPCanMoveInRoom3D<false>(Room, S, &stepend, Height, Speed, ObjectID, moveOutsideBSP, false, false, &blockWall))
      {
         //dprintf("Astar-SKIP-Direct Step (%s)", GetResourceByID(Room->resource_id)->resource_name);
         *P = stepend;

         // flag as direct move towards end
         *Flags |= STATE_MOVE_DIRECT;

         // unset heuristic flags
         *Flags &= ~ESTATE_AVOIDING;
         *Flags &= ~ESTATE_CLOCKWISE;
         return true;
      }
   }

   /****************************************************/
   // 2) can't walk direct line
   /****************************************************/
   float temp1; BspLeaf* temp2;

   // try get next step from astar if enabled and end is inside legal sector
   if (ASTARENABLED && 
       BSPGetHeight(Room, E, &temp1, &temp1, &temp1, &temp2) &&
       AStarGetStepTowards(Room, S, E, P, Flags, ObjectID))
   {
      // flag as pathmoving in piState return
      *Flags |= STATE_MOVE_PATH;

      // unset heuristic flags
      *Flags &= ~ESTATE_AVOIDING;
      *Flags &= ~ESTATE_CLOCKWISE;
      return true;
   }
   else
   {
      /****************************************************/
      // 2.1) try direct step towards destination first
      /****************************************************/
      V2ADD(&stepend, S, &se);
      V2ROUNDROOTOKODFINENESS(&stepend);
      if (BSPCanMoveInRoom3D<false>(Room, S, &stepend, Height, Speed, ObjectID, moveOutsideBSP, false, false, &blockWall))
      {
         *P = stepend;
         *Flags &= ~ESTATE_AVOIDING;
         *Flags &= ~ESTATE_CLOCKWISE;
         return true;
      }

      /****************************************************/
      // 2.2) can't do direct step
      /****************************************************/

      bool isAvoiding = ((*Flags & ESTATE_AVOIDING) == ESTATE_AVOIDING);
      bool isLeft = ((*Flags & ESTATE_CLOCKWISE) == ESTATE_CLOCKWISE);

      // not yet in clockwise or cclockwise mode
      if (!isAvoiding)
      {
         // if not blocked by a wall, roll a dice to decide
         // how to get around the blocking obj.
         if (!blockWall)
            isLeft = (rand() % 2 == 1);

         // blocked by wall, go first into 'slide-along' direction
         // based on vector towards target
         else
         {
            V2 p1p2;
            V2SUB(&p1p2, &blockWall->P2, &blockWall->P1);

            // note: walls can be aligned in any direction like left->right, right->left,
            //   same with up->down and same also with the movement vector.
            //   The typical angle between vectors, acosf(..) is therefore insufficient to differ.
            //   What is done here is a convert into polar-coordinates (= angle in 0..2pi from x-axis)
            //   The difference (or sum) (-2pi..2pi) then provides up to 8 different cases (quadrants) which must be mapped
            //   to the left or right decision.
            float f1 = atan2f(se.Y, se.X);
            float f2 = atan2f(p1p2.Y, p1p2.X);
            float df = f1 - f2;

            bool q1_pos = (df >= 0.0f && df <= (float)M_PI_2);
            bool q2_pos = (df >= (float)M_PI_2 && df <= (float)M_PI);
            bool q3_pos = (df >= (float)M_PI && df <= (float)(M_PI + M_PI_2));
            bool q4_pos = (df >= (float)(M_PI + M_PI_2) && df <= (float)M_PI*2.0f);
            bool q1_neg = (df <= 0.0f && df >= (float)-M_PI_2);
            bool q2_neg = (df <= (float)-M_PI_2 && df >= (float)-M_PI);
            bool q3_neg = (df <= (float)-M_PI && df >= (float)-(M_PI + M_PI_2));
            bool q4_neg = (df <= (float)-(M_PI + M_PI_2) && df >= (float)-M_PI*2.0f);

            isLeft = (q1_pos || q2_pos || q1_neg || q3_neg) ? false : true;

            /*if (isLeft)
               dprintf("trying left first  r: %f", df);
            else
               dprintf("trying right first   r: %f", df);*/
         }
      }

      // must run this possibly twice
      // e.g. left after right failed or right after left failed
      for (int i = 0; i < 2; i++)
      {
         if (isLeft)
         {
            V2 v = se;

            // try rotating move left in 6 steps of 22.5� up to 135�
            for (int j = 0; j < 6; j++)
            {
               V2ROTATE(&v, 0.5f * (float)-M_PI_4);
               V2ADD(&stepend, S, &v);
               V2ROUNDROOTOKODFINENESS(&stepend);
               if (BSPCanMoveInRoom3D<false>(Room, S, &stepend, Height, Speed, ObjectID, moveOutsideBSP, false, false, &blockWall))
               {
                  *P = stepend;
                  *Flags |= ESTATE_AVOIDING;
                  *Flags |= ESTATE_CLOCKWISE;
                  return true;
               }
            }

            // failed to circumvent by going left, switch to right
            isLeft = false;
            *Flags |= ESTATE_AVOIDING;
            *Flags &= ~ESTATE_CLOCKWISE;
         }
         else
         {
            V2 v = se;

            // try rotating move right in 6 steps of 22.5� up to 135�
            for (int j = 0; j < 6; j++)
            {
               V2ROTATE(&v, 0.5f * (float)M_PI_4);
               V2ADD(&stepend, S, &v);
               V2ROUNDROOTOKODFINENESS(&stepend);
               if (BSPCanMoveInRoom3D<false>(Room, S, &stepend, Height, Speed, ObjectID, moveOutsideBSP, false, false, &blockWall))
               {
                  *P = stepend;
                  *Flags |= ESTATE_AVOIDING;
                  *Flags &= ~ESTATE_CLOCKWISE;
                  return true;
               }
            }

            // failed to circumvent by going right, switch to left
            isLeft = true;
            *Flags |= ESTATE_AVOIDING;
            *Flags |= ESTATE_CLOCKWISE;
         }
      }
   }

   /****************************************************/
   // 3) fully stuck
   /****************************************************/

   *P = *S;
   *Flags &= ~ESTATE_AVOIDING;
   *Flags &= ~ESTATE_CLOCKWISE;
   return false;
}

/*********************************************************************************************/
/* BSPBlockerClear:   Clears all registered blocked locations.                               */
/*********************************************************************************************/
void BSPBlockerClear(room_type* Room)
{
   BlockerNode* blocker = Room->Blocker;
   while (blocker)
   {
      BlockerNode* tmp = blocker->Next;
      FreeMemorySIMD(MALLOC_ID_ROOM, blocker, sizeof(BlockerNode));
      blocker = tmp;
   }
   Room->Blocker = NULL;
}

/*********************************************************************************************/
/* BSPBlockerRemove:  Removes a blocked location.                                            */
/*********************************************************************************************/
bool BSPBlockerRemove(room_type* Room, int ObjectID)
{
   if (!Room)
      return false;

   BlockerNode* blocker = Room->Blocker;
   BlockerNode* previous = NULL;

   while (blocker)
   {
      if (blocker->ObjectID == ObjectID)
      {
         // removing first element
         if (!previous)
            Room->Blocker = blocker->Next;

         // removing not first element
         else
            previous->Next = blocker->Next;

         // now cleanup node
         FreeMemorySIMD(MALLOC_ID_ROOM, blocker, sizeof(BlockerNode));

         return true;
      }

      previous = blocker;
      blocker = blocker->Next;
   }

   return false;
}

/*********************************************************************************************/
/* BSPBlockerAdd:     Adds a blocked location in the room.                                   */
/*********************************************************************************************/
bool BSPBlockerAdd(room_type* Room, int ObjectID, V2* P)
{
   // sanity check: these are binary or operations because it's very unlikely
   // any condition is met. So next ones can't be skipped, so binary is faster.
   if (!Room | !P)
      return false;

   // alloc
   BlockerNode* newblocker = (BlockerNode*)AllocateMemorySIMD(MALLOC_ID_ROOM, sizeof(BlockerNode));

   // set values on new blocker
   newblocker->ObjectID = ObjectID;
   newblocker->Position = *P;
   newblocker->TickLastMove = 0;
   newblocker->Next = NULL;
   
   // first blocker
   if (!Room->Blocker)
      Room->Blocker = newblocker;

   else
   {
      // we insert at the beginning because it's
      // (a) faster
      // (b) it makes sure 'static' objects are at the end (unlikely to be touched again)
      newblocker->Next = Room->Blocker;
      Room->Blocker = newblocker;
   }

   return true;
}

/*********************************************************************************************/
/* BSPBlockerMove:    Moves an existing blocked location to somewhere else.                  */
/*********************************************************************************************/
bool BSPBlockerMove(room_type* Room, int ObjectID, V2* P)
{
   // sanity check: these are binary or operations because it's very unlikely
   // any condition is met. So next ones can't be skipped, so binary is faster.
   if (!Room | !P)
      return false;

   BlockerNode* blocker = Room->Blocker;
   while (blocker)
   {
      if (blocker->ObjectID == ObjectID)
      {
         blocker->Position = *P;
         blocker->TickLastMove = (unsigned int)GetMilliCount();
         return true;
      }
      blocker = blocker->Next;
   }

   return false;
}

/*********************************************************************************************/
/* BSPRooFileLoadServer:  Fill "room" with server-relevant data from given roo file.         */
/*********************************************************************************************/
bool BSPLoadRoom(char *fname, room_type *room)
{
   int i, j, temp;
   unsigned char byte;
   unsigned short unsigshort;
   int offset_client, offset_tree, offset_walls, offset_sides, offset_sectors, offset_things;
   char tmpbuf[128];

   FILE *infile = fopen(fname, "rb");
   if (infile == NULL)
      return False;

   /****************************************************************************/
   /*                                HEADER                                    */
   /****************************************************************************/
   
   // check signature
   if (fread(&temp, 1, 4, infile) != 4 || temp != ROO_SIGNATURE)
   { fclose(infile); return False; }

   // check version
   if (fread(&temp, 1, 4, infile) != 4 || temp < ROO_VERSION)
   { fclose(infile); return False; }

   // read room security
   if (fread(&room->security, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   // read pointer to client info
   if (fread(&offset_client, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   // skip pointer to server info
   if (fread(&temp, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   /****************************************************************************/
   /*                               CLIENT DATA                                */
   /****************************************************************************/
   fseek(infile, offset_client, SEEK_SET);
   
   // skip width
   if (fread(&temp, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   // skip height
   if (fread(&temp, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   // read pointer to bsp tree
   if (fread(&offset_tree, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   // read pointer to walls
   if (fread(&offset_walls, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   // skip offset to editor walls
   if (fread(&temp, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   // read pointer to sides
   if (fread(&offset_sides, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   // read pointer to sectors
   if (fread(&offset_sectors, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   // read pointer to things
   if (fread(&offset_things, 1, 4, infile) != 4)
   { fclose(infile); return False; }

   /************************ BSP-TREE ****************************************/

   fseek(infile, offset_tree, SEEK_SET);

   // read count of nodes
   if (fread(&room->TreeNodesCount, 1, 2, infile) != 2)
   { fclose(infile); return False; }

   // allocate tree mem
   room->TreeNodes = (BspNode*)AllocateMemorySIMD(
      MALLOC_ID_ROOM, room->TreeNodesCount * sizeof(BspNode));

   for (i = 0; i < room->TreeNodesCount; i++)
   {
      BspNode* node = &room->TreeNodes[i];

      // type
      if (fread(&byte, 1, 1, infile) != 1)
      { fclose(infile); return False; }
      node->Type = (BspNodeType)byte;

      // boundingbox
      if (fread(&node->BoundingBox.Min.X, 1, 4, infile) != 4)
      { fclose(infile); return False; }
      if (fread(&node->BoundingBox.Min.Y, 1, 4, infile) != 4)
      { fclose(infile); return False; }
      if (fread(&node->BoundingBox.Max.X, 1, 4, infile) != 4)
      { fclose(infile); return False; }
      if (fread(&node->BoundingBox.Max.Y, 1, 4, infile) != 4)
      { fclose(infile); return False; }

      if (node->Type == BspInternalType)
      {
         // line equation coefficients of splitter line
         if (fread(&node->u.internal.A, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&node->u.internal.B, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&node->u.internal.C, 1, 4, infile) != 4)
         { fclose(infile); return False; }

         // nums of children
         if (fread(&node->u.internal.RightChildNum, 1, 2, infile) != 2)
         { fclose(infile); return False; }
         if (fread(&node->u.internal.LeftChildNum, 1, 2, infile) != 2)
         { fclose(infile); return False; }

         // first wall in splitter
         if (fread(&node->u.internal.FirstWallNum, 1, 2, infile) != 2)
         { fclose(infile); return False; }
      }
      else if (node->Type == BspLeafType)
      {
         // sector num
         if (fread(&node->u.leaf.SectorNum, 1, 2, infile) != 2)
         { fclose(infile); return False; }

         // points count
         if (fread(&node->u.leaf.PointsCount, 1, 2, infile) != 2)
         { fclose(infile); return False; }

         // allocate memory for points of polygon
         node->u.leaf.PointsFloor = (V3*)AllocateMemorySIMD(
            MALLOC_ID_ROOM, node->u.leaf.PointsCount * sizeof(V3));
         node->u.leaf.PointsCeiling = (V3*)AllocateMemorySIMD(
            MALLOC_ID_ROOM, node->u.leaf.PointsCount * sizeof(V3));

         // read points
         for (j = 0; j < node->u.leaf.PointsCount; j++)
         {
            if (fread(&node->u.leaf.PointsFloor[j].X, 1, 4, infile) != 4)
            { fclose(infile); return False; }
            if (fread(&node->u.leaf.PointsFloor[j].Y, 1, 4, infile) != 4)
            { fclose(infile); return False; }
			   
            // x,y are same on floor/ceiling, set yet unknown Z and unused W to 0.0f
            node->u.leaf.PointsCeiling[j].X = node->u.leaf.PointsFloor[j].X;
            node->u.leaf.PointsCeiling[j].Y = node->u.leaf.PointsFloor[j].Y;
            node->u.leaf.PointsCeiling[j].Z = node->u.leaf.PointsFloor[j].Z = 0.0f;
#if defined(SSE2) || defined(SSE4)
            node->u.leaf.PointsCeiling[j].W = node->u.leaf.PointsFloor[j].W = 0.0f;
#endif
         }
      }
   }

   /*************************** WALLS ****************************************/
   
   fseek(infile, offset_walls, SEEK_SET);

   // count of walls
   if (fread(&room->WallsCount, 1, 2, infile) != 2)
   { fclose(infile); return False; }

   // allocate walls mem
   room->Walls = (Wall*)AllocateMemorySIMD(
      MALLOC_ID_ROOM, room->WallsCount * sizeof(Wall));

   for (i = 0; i < room->WallsCount; i++)
   {
      Wall* wall = &room->Walls[i];

      // save 1-based num for debugging
      wall->Num = i + 1;

      // nextwallinplane num
      if (fread(&wall->NextWallInPlaneNum, 1, 2, infile) != 2)
      { fclose(infile); return False; }

      // side nums
      if (fread(&wall->RightSideNum, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      if (fread(&wall->LeftSideNum, 1, 2, infile) != 2)
      { fclose(infile); return False; }

      // endpoints
      if (fread(&wall->P1.X, 1, 4, infile) != 4)
      { fclose(infile); return False; }
      if (fread(&wall->P1.Y, 1, 4, infile) != 4)
      { fclose(infile); return False; }
      if (fread(&wall->P2.X, 1, 4, infile) != 4)
      { fclose(infile); return False; }
      if (fread(&wall->P2.Y, 1, 4, infile) != 4)
      { fclose(infile); return False; }

      // skip length
      if (fread(&temp, 1, 4, infile) != 4)
      { fclose(infile); return False; }

      // skip texture offsets
      if (fread(&temp, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      if (fread(&temp, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      if (fread(&temp, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      if (fread(&temp, 1, 2, infile) != 2)
      { fclose(infile); return False; }

      // sector nums
      if (fread(&wall->RightSectorNum, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      if (fread(&wall->LeftSectorNum, 1, 2, infile) != 2)
      { fclose(infile); return False; }
   }

   /***************************** SIDES ****************************************/

   fseek(infile, offset_sides, SEEK_SET);

   // count of sides
   if (fread(&room->SidesCount, 1, 2, infile) != 2)
   { fclose(infile); return False; }

   // allocate sides mem
   room->Sides = (Side*)AllocateMemory(
      MALLOC_ID_ROOM, room->SidesCount * sizeof(Side));

   for (i = 0; i < room->SidesCount; i++)
   {
      Side* side = &room->Sides[i];

      // serverid
      if (fread(&side->ServerID, 1, 2, infile) != 2)
      { fclose(infile); return False; }

      // middle,upper,lower texture
      if (fread(&side->TextureMiddle, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      if (fread(&side->TextureUpper, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      if (fread(&side->TextureLower, 1, 2, infile) != 2)
      { fclose(infile); return False; }

      // keep track of original texture nums (can change at runtime)
	  side->TextureLowerOrig  = side->TextureLower;
	  side->TextureMiddleOrig = side->TextureMiddle;
	  side->TextureUpperOrig  = side->TextureUpper;

      // flags
     if (fread(&side->Flags, 1, 4, infile) != 4)
      { fclose(infile); return False; }

      // skip speed byte
     if (fread(&temp, 1, 1, infile) != 1)
      { fclose(infile); return False; }
   }

   /***************************** SECTORS ****************************************/

   fseek(infile, offset_sectors, SEEK_SET);

   // count of sectors
   if (fread(&room->SectorsCount, 1, 2, infile) != 2)
   { fclose(infile); return False; }

   // allocate sectors mem
   room->Sectors = (SectorNode*)AllocateMemory(
      MALLOC_ID_ROOM, room->SectorsCount * sizeof(SectorNode));

   for (i = 0; i < room->SectorsCount; i++)
   {
      SectorNode* sector = &room->Sectors[i];
	   
      // serverid
      if (fread(&sector->ServerID, 1, 2, infile) != 2)
      { fclose(infile); return False; }

      // floor+ceiling texture
      if (fread(&sector->FloorTexture, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      if (fread(&sector->CeilingTexture, 1, 2, infile) != 2)
      { fclose(infile); return False; }

      // keep track of original texture nums (can change at runtime)
      sector->FloorTextureOrig   = sector->FloorTexture;
      sector->CeilingTextureOrig = sector->CeilingTexture;

      // skip texture offsets
      if (fread(&temp, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      if (fread(&temp, 1, 2, infile) != 2)
      { fclose(infile); return False; }

      // floor+ceiling heights (from 1:64 to 1:1024 like the rest)
      if (fread(&unsigshort, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      sector->FloorHeight = FINENESSKODTOROO((float)unsigshort);
      if (fread(&unsigshort, 1, 2, infile) != 2)
      { fclose(infile); return False; }
      sector->CeilingHeight = FINENESSKODTOROO((float)unsigshort);

      // skip light byte
      if (fread(&temp, 1, 1, infile) != 1)
      { fclose(infile); return False; }

      // flags
      if (fread(&sector->Flags, 1, 4, infile) != 4)
      { fclose(infile); return False; }

      // Keep track of original flags (can change at runtime)
      sector->FlagsOrig = sector->Flags;

      // skip speed byte
      if (fread(&temp, 1, 1, infile) != 1)
      { fclose(infile); return False; }
	   
      // possibly load floor slopeinfo
      if ((sector->Flags & SF_SLOPED_FLOOR) == SF_SLOPED_FLOOR)
      {
         sector->SlopeInfoFloor = (SlopeInfo*)AllocateMemory(
            MALLOC_ID_ROOM, sizeof(SlopeInfo));

         // read 3d plane equation coefficients (normal vector)
         if (fread(&sector->SlopeInfoFloor->A, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&sector->SlopeInfoFloor->B, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&sector->SlopeInfoFloor->C, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&sector->SlopeInfoFloor->D, 1, 4, infile) != 4)
         { fclose(infile); return False; }

         // skip x0, y0, textureangle
         if (fread(&temp, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&temp, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&temp, 1, 4, infile) != 4)
         { fclose(infile); return False; }

         // skip unused payload (vertex indices for roomedit)
         if (fread(&tmpbuf, 1, 18, infile) != 18)
         { fclose(infile); return False; }
      }
      else
         sector->SlopeInfoFloor = NULL;

      // possibly load ceiling slopeinfo
      if ((sector->Flags & SF_SLOPED_CEILING) == SF_SLOPED_CEILING)
      {
         sector->SlopeInfoCeiling = (SlopeInfo*)AllocateMemory(
            MALLOC_ID_ROOM, sizeof(SlopeInfo));

         // read 3d plane equation coefficients (normal vector)
         if (fread(&sector->SlopeInfoCeiling->A, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&sector->SlopeInfoCeiling->B, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&sector->SlopeInfoCeiling->C, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&sector->SlopeInfoCeiling->D, 1, 4, infile) != 4)
         { fclose(infile); return False; }

         // skip x0, y0, textureangle
         if (fread(&temp, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&temp, 1, 4, infile) != 4)
         { fclose(infile); return False; }
         if (fread(&temp, 1, 4, infile) != 4)
         { fclose(infile); return False;}

         // skip unused payload (vertex indices for roomedit)
         if (fread(&tmpbuf, 1, 18, infile) != 18)
         { fclose(infile); return False; }
      }
      else
         sector->SlopeInfoCeiling = NULL;
   }

   /***************************** THINGS ****************************************/
   
   fseek(infile, offset_things, SEEK_SET);

   // count of things
   if (fread(&unsigshort, 1, 2, infile) != 2)
   { fclose(infile); return False; }

   // must have exactly two things describing bbox (each thing a vertex)
   if (unsigshort != 2)
   { fclose(infile); return False; }

   // note: Things vertices are stored as INT in (1:64) fineness, based on the
   // coordinate-system origin AS SHOWN IN ROOMEDIT (Y-UP).
   // Also these can be ANY variant of the 2 possible sets describing
   // a diagonal in a rectangle, so not guaranteed to be ordered like min/or max first.
   float x0, x1, y0, y1;

   if (fread(&temp, 1, 4, infile) != 4)
   { fclose(infile); return False; }
   x0 = (float)temp;
   if (fread(&temp, 1, 4, infile) != 4)
   { fclose(infile); return False; }
   y0 = (float)temp;
   if (fread(&temp, 1, 4, infile) != 4)
   { fclose(infile); return False; }
   x1 = (float)temp;
   if (fread(&temp, 1, 4, infile) != 4)
   { fclose(infile); return False; }
   y1 = (float)temp;


   // from the 4 bbox points shown in roomedit (defined by 2 vertices)
   // 1) Pick the left-bottom one as minimum (and scale to ROO fineness)
   // 2) Pick the right-up one as maximum (and scale to ROO fineness)
   room->ThingsBox.Min.X = FINENESSKODTOROO(fmin(x0, x1));
   room->ThingsBox.Min.Y = FINENESSKODTOROO(fmin(y0, y1));
   room->ThingsBox.Max.X = FINENESSKODTOROO(fmax(x0, x1));
   room->ThingsBox.Max.Y = FINENESSKODTOROO(fmax(y0, y1));

   // when roomedit saves the ROO, it translates the origin (0/0)
   // into one boundingbox point, so that origin in ROO (0/0)
   // later is roughly equal to (row=1 col=1)
   
   // translate box so minimum is at (0/0)
   room->ThingsBox.Max.X = room->ThingsBox.Max.X - room->ThingsBox.Min.X;
   room->ThingsBox.Max.Y = room->ThingsBox.Max.Y - room->ThingsBox.Min.Y;
   room->ThingsBox.Min.X = 0.0f;
   room->ThingsBox.Min.Y = 0.0f;

   // calculate the old cols/rows values rather than loading them
   room->cols = (int)(room->ThingsBox.Max.X / 1024.0f);
   room->rows = (int)(room->ThingsBox.Max.Y / 1024.0f);
   room->colshighres = (int)(room->ThingsBox.Max.X / 256.0f);
   room->rowshighres = (int)(room->ThingsBox.Max.Y / 256.0f);

   /************************** DONE READNG **********************************/

   fclose(infile);

   /*************************************************************************/
   /*                      RESOLVE NUMS TO POINTERS                         */
   /*************************************************************************/

   // walls
   for (int i = 0; i < room->WallsCount; i++)
   {
      Wall* wall = &room->Walls[i];

      // right sector
      if (wall->RightSectorNum > 0 &&
          room->SectorsCount > wall->RightSectorNum - 1)
            wall->RightSector = &room->Sectors[wall->RightSectorNum - 1];
      else
         wall->RightSector = NULL;

      // left sector
      if (wall->LeftSectorNum > 0 &&
         room->SectorsCount > wall->LeftSectorNum - 1)
            wall->LeftSector = &room->Sectors[wall->LeftSectorNum - 1];
      else
         wall->LeftSector = NULL;

      // right side
      if (wall->RightSideNum > 0 &&
         room->SidesCount > wall->RightSideNum - 1)
            wall->RightSide = &room->Sides[wall->RightSideNum - 1];
      else
         wall->RightSide = NULL;

      // left side
      if (wall->LeftSideNum > 0 &&
         room->SidesCount > wall->LeftSideNum - 1)
            wall->LeftSide = &room->Sides[wall->LeftSideNum - 1];
      else
         wall->LeftSide = NULL;

      // next wall in splitter
      if (wall->NextWallInPlaneNum > 0 &&
         room->WallsCount > wall->NextWallInPlaneNum - 1)
            wall->NextWallInPlane = &room->Walls[wall->NextWallInPlaneNum - 1];
      else
         wall->NextWallInPlane = NULL;
   }

   // bsp nodes
   for (int i = 0; i < room->TreeNodesCount; i++)
   {
      BspNode* node = &room->TreeNodes[i];

      // internal nodes
      if (node->Type == BspInternalType)
      {
         // first wall
         if (node->u.internal.FirstWallNum > 0 &&
             room->WallsCount > node->u.internal.FirstWallNum - 1)
               node->u.internal.FirstWall = &room->Walls[node->u.internal.FirstWallNum - 1];
         else
            node->u.internal.FirstWall = NULL;

         // right child
         if (node->u.internal.RightChildNum > 0 &&
             room->TreeNodesCount > node->u.internal.RightChildNum - 1)
               node->u.internal.RightChild = &room->TreeNodes[node->u.internal.RightChildNum - 1];
         else
            node->u.internal.RightChild = NULL;

         // left child
         if (node->u.internal.LeftChildNum > 0 &&
             room->TreeNodesCount > node->u.internal.LeftChildNum - 1)
               node->u.internal.LeftChild = &room->TreeNodes[node->u.internal.LeftChildNum - 1];
         else
            node->u.internal.LeftChild = NULL;
      }

      // leafs
      else if (node->Type == BspLeafType)
      {
         // sector this leaf belongs to
         if (node->u.leaf.SectorNum > 0 &&
             room->SectorsCount > node->u.leaf.SectorNum - 1)
               node->u.leaf.Sector = &room->Sectors[node->u.leaf.SectorNum - 1];
         else
            node->u.leaf.Sector = NULL;
      }
   }

   /*************************************************************************/
   /*                RESOLVE HEIGHTS OF LEAF POLY POINTS                    */
   /*                AND NORMALIZE SPLITTER KOEFFICIENTS                    */
   /*************************************************************************/

   for (int i = 0; i < room->TreeNodesCount; i++)
   {
      BspNode* node = &room->TreeNodes[i];

      if (node->Type == BspLeafType)
      {
         for (int j = 0; j < node->u.leaf.PointsCount; j++)
         {
            if (!node->u.leaf.Sector)
               continue;

            V2 p = { node->u.leaf.PointsFloor[j].X, node->u.leaf.PointsFloor[j].Y };

            node->u.leaf.PointsFloor[j].Z = 
               SECTORHEIGHTFLOOR(node->u.leaf.Sector, &p);

            node->u.leaf.PointsCeiling[j].Z =
               SECTORHEIGHTCEILING(node->u.leaf.Sector, &p);
         }
      }
      else if (node->Type == BspInternalType)
      {
         const float len = sqrtf(
            node->u.internal.A * node->u.internal.A +
            node->u.internal.B * node->u.internal.B);

         // normalize
         if (!ISZERO(len))
         {
            node->u.internal.A /= len;
            node->u.internal.B /= len;
            node->u.internal.C /= len;
         }

         // should never be reached for valid maps
         else
            dprintf("INVALID SPLITTER KOEFFICIENTS IN ROOM");
      }
   }

   /****************************************************************************/
   /****************************************************************************/

   // no initial blockers
   room->Blocker = NULL;

   // depth override settings
   room->DepthFlags     = 0;
   room->OverrideDepth1 = 0;
   room->OverrideDepth2 = 0;
   room->OverrideDepth3 = 0;

   /****************************************************************************/
   /****************************************************************************/

#if ASTARENABLED
   // calculate size of edgecache
   room->EdgesCacheSize = room->colshighres * room->rowshighres * sizeof(unsigned short);

   // allocate memory for edge cache (BSP queries between squares)
   room->EdgesCache = (unsigned short*)AllocateMemory(
      MALLOC_ID_ASTAR, room->EdgesCacheSize);

   // clear edge cache
   AStarClearEdgesCache(room);

 #if PREBUILDEDGECACHE
   // prebuild edge cache
   AStarBuildEdgesCache(room);
 #endif

   // allocate path cache
   for (int i = 0; i < PATHCACHESIZE; i++)
   {
      room->Paths[i] = (astar_path*)AllocateMemory(MALLOC_ID_ASTAR, sizeof(astar_path));

      // do a full zero initialization on pathcache
      // it's only minimally cleared at runtime
      ZeroMemory(room->Paths[i], sizeof(astar_path));
   }

   // allocate nopath cache
   for (int i = 0; i < NOPATHCACHESIZE; i++)
   {
      room->NoPaths[i] = (astar_nopath*)AllocateMemory(MALLOC_ID_ASTAR, sizeof(astar_nopath));

      // do a full zero initialization on nopatch
      // it's only minimally cleared at runtime
      ZeroMemory(room->NoPaths[i], sizeof(astar_nopath));
   }

   // clear path cache
   AStarClearPathCaches(room);
#endif

   /****************************************************************************/
   /****************************************************************************/

   return True;
}

/*********************************************************************************************/
/* BSPRoomFreeServer:  Free the parts of a room structure used by the server.                */
/*********************************************************************************************/
void BSPFreeRoom(room_type *room)
{
   int i;

   /****************************************************************************/
   /*                               CLIENT PARTS                               */
   /****************************************************************************/
   
   // free bsp nodes 'submem'
   for (i = 0; i < room->TreeNodesCount; i++)
   {
      if (room->TreeNodes[i].Type == BspLeafType)
      {
         FreeMemorySIMD(MALLOC_ID_ROOM, room->TreeNodes[i].u.leaf.PointsFloor,
            room->TreeNodes[i].u.leaf.PointsCount * sizeof(V3));
         FreeMemorySIMD(MALLOC_ID_ROOM, room->TreeNodes[i].u.leaf.PointsCeiling,
            room->TreeNodes[i].u.leaf.PointsCount * sizeof(V3));
      }
   }

   // free sectors submem
   for (i = 0; i < room->SectorsCount; i++)
   {
      if ((room->Sectors[i].Flags & SF_SLOPED_FLOOR) == SF_SLOPED_FLOOR)
         FreeMemory(MALLOC_ID_ROOM, room->Sectors[i].SlopeInfoFloor, sizeof(SlopeInfo));
      if ((room->Sectors[i].Flags & SF_SLOPED_CEILING) == SF_SLOPED_CEILING)
         FreeMemory(MALLOC_ID_ROOM, room->Sectors[i].SlopeInfoCeiling, sizeof(SlopeInfo));
   }

   FreeMemorySIMD(MALLOC_ID_ROOM, room->TreeNodes, room->TreeNodesCount * sizeof(BspNode));
   FreeMemorySIMD(MALLOC_ID_ROOM, room->Walls, room->WallsCount * sizeof(Wall));
   FreeMemory(MALLOC_ID_ROOM, room->Sides, room->SidesCount * sizeof(Side));
   FreeMemory(MALLOC_ID_ROOM, room->Sectors, room->SectorsCount * sizeof(SectorNode));

   room->TreeNodesCount = 0;
   room->WallsCount = 0;
   room->SidesCount = 0;
   room->SectorsCount = 0;

#if ASTARENABLED
   // free edgescache mem
   FreeMemory(MALLOC_ID_ASTAR, room->EdgesCache, room->EdgesCacheSize);

   // free path cache
   for (int i = 0; i < PATHCACHESIZE; i++)
      FreeMemory(MALLOC_ID_ASTAR, room->Paths[i], sizeof(astar_path));

   // free nopath cache
   for (int i = 0; i < NOPATHCACHESIZE; i++)
      FreeMemory(MALLOC_ID_ASTAR, room->NoPaths[i], sizeof(astar_nopath));
#endif

   BSPBlockerClear(room);

   /****************************************************************************/
   /*                               SERVER PARTS                               */
   /****************************************************************************/
  
   room->rows = 0;
   room->cols = 0;
   room->rowshighres = 0;
   room->colshighres = 0;
   room->resource_id = 0;
   room->roomdata_id = 0;
}
#pragma endregion
