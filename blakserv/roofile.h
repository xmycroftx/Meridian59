// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * roofile.h
 *
 */

#ifndef _ROOFILE_H
#define _ROOFILE_H

#pragma region Macros
/**************************************************************************************************************/
/*                                           MACROS                                                           */
/**************************************************************************************************************/
#define DEBUGLOS              0                      // switch to 1 to enable debug output for BSP LOS
#define DEBUGLOSVIEW          0                      // switch to 1 to enable debug output for directional LOS
#define DEBUGMOVE             0                      // switch to 1 to enable debug output for BSP MOVES
#define ROO_VERSION           14                     // required roo fileformat version (V14 = floatingpoint)
#define ROO_SIGNATURE         0xB14F4F52             // signature of ROO files (first 4 bytes)
#define OBJECTHEIGHTROO       768                    // estimated object height (used in LOS and MOVE calcs)
#define ROOFINENESS           1024.0f                // fineness used in ROO files
#define HALFROOFINENESS       512.0f                 // half ROO fineness for frustum calculations
#define FINENESSKODTOROO(x)   ((x) * 16.0f)          // scales a scalar value from KOD fineness to ROO fineness
#define FINENESSROOTOKOD(x)   ((x) * 0.0625f)        // scales a scalar value from ROO fineness to KOD fineness
#define V3FINENESSKODTOROO(x) V3SCALE((x), 16.0f)    // scales a V3 instance from KOD fineness to ROO fineness
#define V3FINENESSROOTOKOD(x) V3SCALE((x), 0.0625f)  // scales a V3 instance from ROO fineness to KOD fineness
#define V2FINENESSKODTOROO(x) V2SCALE((x), 16.0f)    // scales a V2 instance from KOD fineness to ROO fineness
#define V2FINENESSROOTOKOD(x) V2SCALE((x), 0.0625f)  // scales a V2 instance from ROO fineness to KOD fineness
#define MAX_KOD_DEGREE        4096.0f                // max value of KOD angle
#define HALFFRUSTUMWIDTH      600.0f                 // half player view frustum, * ~1.5x to account for latency
#define MAXINTERSECTIONS      2048                   // max. trackable intersections in BSPCanMoveInRoom3D
#define NOBLOCKOBJUSERDELAY   2000                   // ms since last objmove to cause user-move validations

#define SPEEDKODTOROO(x)      ((x) * ROOFINENESS * 0.0001f)             // converts big.sq. per 10s in ROO fine units per 1ms
#define GRAVITYACCELERATION   FINENESSKODTOROO(-0.00032f)               // dist/ms� (dist=1:1024)
#define MAXSTEPHEIGHT         ((float)(24 << 4))                        // (from clientd3d/move.c)
#define MAXSTEPHEIGHTUSER     (MAXSTEPHEIGHT + 96)                      // for user validation, with some tolerance
#define PLAYERWIDTH           (31.0f * (float)KODFINENESS * 0.25f)      // (from clientd3d/game.c)
#define WALLMINDISTANCE       (PLAYERWIDTH / 2.0f)                      // (from clientd3d/game.c)
#define WALLMINDISTANCE2      (WALLMINDISTANCE * WALLMINDISTANCE)       // (from clientd3d/game.c)
#define WALLTOLERANCEUSER     128                                       // 128 ROO units = 8 KOD fine units
#define WALLMINDISTANCEUSER   (WALLMINDISTANCE - WALLTOLERANCEUSER)     // let users get a bit closer (tolerance for check)
#define WALLMINDISTANCEUSER2  (WALLMINDISTANCEUSER*WALLMINDISTANCEUSER) // squared WALLMINDISTANCEUSER
#define OBJMINDISTANCE        512.0f                                    // 2 astar rows/cols, MUST BE MULTIPLE OF 256
#define OBJMINDISTANCE2       (OBJMINDISTANCE * OBJMINDISTANCE)         // squared OBJMINDISTANCE
#define OBJTOLERANCEUSER      96                                        // 96 ROO units = 6 KOD fine units (max 256!)
#define OBJMINDISTANCEUSER    (ROOFINENESS/4 - OBJTOLERANCEUSER)        // See MIN_NOMOVEON in 'clientd3d/move.c'
#define OBJMINDISTANCEUSER2   (OBJMINDISTANCEUSER * OBJMINDISTANCEUSER) // squared OBJMINDISTANCEUSER
#define LOSEXTEND             64.0f
#define MAXRNDMOVEDELTAH      (5.0f * MAXSTEPHEIGHT)                    // ignore rnd gen move dest pnt if above/below this
#define MAXRNDMOVEDELTAH2     (MAXRNDMOVEDELTAH * MAXRNDMOVEDELTAH)     // squared MAXRNDMOVEDELTAH

// Calculation to convert KOD angles to radians.
#define KODANGLETORADIANS(x) ((float)((x) % (int)MAX_KOD_DEGREE) * PI_MULT_2 / MAX_KOD_DEGREE)

// converts grid coordinates
// input: 1-based value of big row (or col), 0-based value of fine-row (or col)
// output: 0-based value in ROO fineness
#define GRIDCOORDTOROO(big, fine) \
   (FINENESSKODTOROO((float)(big - 1) * (float)KODFINENESS + (float)fine))

#define ROOCOORDTOGRIDBIG(x)  (((int)FINENESSROOTOKOD(x) / KODFINENESS)+1)
#define ROOCOORDTOGRIDFINE(x) ((int)FINENESSROOTOKOD(x) % KODFINENESS)

// converts a floatingpoint-value into KOD integer (boxes into MAX/MIN KOD INT)
#define FLOATTOKODINT(x) \
   (((x) > (float)MAX_KOD_INT) ? MAX_KOD_INT : (((x) < (float)-MIN_KOD_INT) ? -MIN_KOD_INT : (int)x))

// rounds a floatingpoint-value in ROO fineness to next close
// value exactly expressable in KOD fineness units
#define ROUNDROOTOKODFINENESS(a)   FINENESSKODTOROO(roundf(FINENESSROOTOKOD(a)))

// rounds a V3 instance in ROO fineness to next close
// value exactly expressable in KOD fineness units
#define V3ROUNDROOTOKODFINENESS(a) \
  V3FINENESSROOTOKOD(a);           \
  V3ROUND(a);                      \
  V3FINENESSKODTOROO(a);

// rounds a V2 instance in ROO fineness to next close
// value exactly expressable in KOD fineness units
#define V2ROUNDROOTOKODFINENESS(a) \
  V2FINENESSROOTOKOD(a);           \
  V2ROUND(a);                      \
  V2FINENESSKODTOROO(a);

// from blakston.khd, set on special move-returns
#define STATE_MOVE_PATH    0x00000020
#define STATE_MOVE_DIRECT  0x00000040

// from blakston.khd, used in BSPGetNextStepTowards across calls
#define ESTATE_LONG_STEP 0x00002000
#define ESTATE_AVOIDING  0x00004000
#define ESTATE_CLOCKWISE 0x00008000

// from blakston.khd, used for monster that can move outside BSP tree
#define MSTATE_MOVE_OUTSIDE_BSP 0x00100000

// query flags for BSPGetLocationInfo
#define LIQ_GET_SECTORINFO           0x00000001
#define LIQ_CHECK_THINGSBOX          0x00000002
#define LIQ_CHECK_OBJECTBLOCK        0x00000004

// return flags for BSPGetLocationInfo
#define LIR_TBOX_OUT_N       0x00000001
#define LIR_TBOX_OUT_E       0x00000002
#define LIR_TBOX_OUT_S       0x00000004
#define LIR_TBOX_OUT_W       0x00000008
#define LIR_TBOX_OUT_NE      0x00000003 //N+E
#define LIR_TBOX_OUT_SE      0x00000006 //S+E
#define LIR_TBOX_OUT_NW      0x00000009 //N+W
#define LIR_TBOX_OUT_SW      0x0000000C //S+W
#define LIR_SECTOR_INSIDE    0x00000010
#define LIR_SECTOR_HASFTEX   0x00000020
#define LIR_SECTOR_HASCTEX   0x00000040
#define LIR_SECTOR_NOMOVE    0x00000080
#define LIR_SECTOR_DEPTHMASK 0x00000300
#define LIR_SECTOR_DEPTH0    0x00000000
#define LIR_SECTOR_DEPTH1    0x00000100
#define LIR_SECTOR_DEPTH2    0x00000200
#define LIR_SECTOR_DEPTH3    0x00000300
#define LIR_BLOCKED_OBJECT   0x00000400

// Parameter flags for CanMoveInRoomBSP
#define CANMOVE_IS_PLAYER        0x00000001
#define CANMOVE_MOVE_OUTSIDE_BSP 0x00000002
#define CANMOVE_IGNORE_BLOCKERS  0x00000004
#pragma endregion

#pragma region Structs
/**************************************************************************************************************/
/*                                          STRUCTS                                                           */
/**************************************************************************************************************/
typedef struct BoundingBox2D
{
   V2 Min;
   V2 Max;
} BoundingBox2D;

typedef struct Side
{
   unsigned short ServerID;
   unsigned short TextureMiddle;
   unsigned short TextureUpper;
   unsigned short TextureLower;
   unsigned int   Flags;
   unsigned short TextureMiddleOrig;
   unsigned short TextureUpperOrig;
   unsigned short TextureLowerOrig;
} Side;

typedef struct SlopeInfo
{
   float A;
   float B;
   float C;
   float D;
} SlopeInfo;

typedef struct SectorNode
{
   unsigned short ServerID;
   unsigned short FloorTexture;
   unsigned short CeilingTexture;
   float          FloorHeight;
   float          CeilingHeight;
   unsigned int   Flags;
   unsigned int   FlagsOrig;
   SlopeInfo*     SlopeInfoFloor;
   SlopeInfo*     SlopeInfoCeiling;
   unsigned short FloorTextureOrig;
   unsigned short CeilingTextureOrig;
} SectorNode;

typedef struct Wall
{
   unsigned short Num;
   unsigned short NextWallInPlaneNum;
   unsigned short RightSideNum;
   unsigned short LeftSideNum;
   V2             P1;
   V2             P2;
   unsigned short RightSectorNum;
   unsigned short LeftSectorNum;
   SectorNode*    RightSector;
   SectorNode*    LeftSector;
   Side*          RightSide;
   Side*          LeftSide;
   Wall*          NextWallInPlane;
} Wall;

typedef enum BspNodeType
{
   BspInternalType = 1,
   BspLeafType = 2
} BspNodeType;

typedef struct BspInternal
{
   float           A;
   float           B;
   float           C;
   unsigned short  RightChildNum;
   unsigned short  LeftChildNum;
   unsigned short  FirstWallNum;
   struct BspNode* RightChild;
   struct BspNode* LeftChild;
   Wall*           FirstWall;
} BspInternal;

typedef struct BspLeaf
{
   unsigned short SectorNum;
   unsigned short PointsCount;
   V3*            PointsFloor;
   V3*            PointsCeiling;
   SectorNode*    Sector;
} BspLeaf;

typedef struct BspNode
{
   BspNodeType    Type;
   BoundingBox2D  BoundingBox;

   union
   {
      BspInternal internal;
      BspLeaf     leaf;
   } u;

} BspNode;

typedef struct BlockerNode
{
   int ObjectID;
   V2 Position;
   unsigned int TickLastMove;
   BlockerNode* Next;
} BlockerNode;

typedef struct Intersection
{
   SectorNode* SectorS;
   SectorNode* SectorE;
   Side* SideS;
   Side* SideE;
   V2 Q;
   float FloorHeight;
   float Distance2;
} Intersection;

typedef struct Intersections
{
   Intersection  Data[MAXINTERSECTIONS];
   Intersection* Ptrs[MAXINTERSECTIONS];
   unsigned int  Size;
} Intersections;

typedef struct room_type
{
   int roomdata_id; 
   int resource_id;
   
   int rows;             /* Size of room in grid squares */
   int cols;
   int rowshighres;      /* Size of room in highres grid squares */
   int colshighres;

   int security;         /* Security number, to ensure that client loads the correct roo file */
   
   BoundingBox2D  ThingsBox;
   BlockerNode*   Blocker;

   BspNode*       TreeNodes;
   unsigned short TreeNodesCount;
   Wall*          Walls;
   unsigned short WallsCount;
   Side*          Sides;
   unsigned short SidesCount;
   SectorNode*    Sectors;
   unsigned short SectorsCount;

   unsigned int DepthFlags;
   float        OverrideDepth1;
   float        OverrideDepth2;
   float        OverrideDepth3;

#if ASTARENABLED
   unsigned short* EdgesCache;
   int             EdgesCacheSize;
   astar_path*     Paths[PATHCACHESIZE];
   unsigned int    NextPathIdx;
   astar_nopath*   NoPaths[NOPATHCACHESIZE];
   unsigned int    NextNoPathIdx;
#endif
} room_type;
#pragma endregion

#pragma region Methods
/**************************************************************************************************************/
/*                                          METHODS                                                           */
/**************************************************************************************************************/
bool  BSPGetHeight(room_type* Room, V2* P, float* HeightF, float* HeightFWD, float* HeightC, BspLeaf** Leaf);
bool  BSPCanMoveInRoom(room_type* Room, V2* S, V2* E, int ObjectID, bool moveOutsideBSP, bool ignoreBlockers, bool ignoreEndBlocker, Wall** BlockWall);
template <bool ISPLAYER> extern bool BSPCanMoveInRoom3D(room_type* Room, V2* S, V2* E, float Height, float Speed, int ObjectID, bool moveOutsideBSP, bool ignoreBlockers, bool ignoreEndBlocker, Wall** BlockWall);
bool  BSPLineOfSightView(V2 *S, V2 *E, int kod_angle);
bool  BSPLineOfSight(room_type* Room, V3* S, V3* E);
bool  BSPLineOfSightTree(const BspNode* Node, const V3* S, const V3* E);
void  BSPChangeTexture(room_type* Room, unsigned int ServerID, unsigned short NewTexture, unsigned int Flags);
void  BSPChangeSectorFlag(room_type* Room, unsigned int ServerID, unsigned int ChangeFlag);
void  BSPMoveSector(room_type* Room, unsigned int ServerID, bool Floor, float Height, float Speed);
bool  BSPGetSectorHeightByID(room_type* Room, unsigned int ServerID, bool Floor, float* Height);
bool  BSPGetLocationInfo(room_type* Room, V2* P, unsigned int QueryFlags, unsigned int* ReturnFlags, float* HeightF, float* HeightFWD, float* HeightC, BspLeaf** Leaf);
bool  BSPGetRandomPoint(room_type* Room, int MaxAttempts, float UnblockedRadius, V2* P);
bool  BSPGetRandomMoveDest(room_type* Room, int MaxAttempts, float MinDistance, float MaxDistance, V2* S, V2* E);
bool  BSPGetStepTowards(room_type* Room, V2* S, V2* E, V2* P, unsigned int* Flags, int ObjectID, float Speed, float Height);
bool  BSPBlockerAdd(room_type* Room, int ObjectID, V2* P);
bool  BSPBlockerMove(room_type* Room, int ObjectID, V2* P);
bool  BSPBlockerRemove(room_type* Room, int ObjectID);
void  BSPBlockerClear(room_type* Room);
bool  BSPLoadRoom(char *fname, room_type *room);
void  BSPFreeRoom(room_type *room);
#pragma endregion

#endif
