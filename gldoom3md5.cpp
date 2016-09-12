#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
//#include <windows.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef WIN32
#include "freeglut/include/GL/freeglut.h"
#else
#include <GL/freeglut.h>
#endif

#define PI 3.14159265358979323846f

static int oldtime;
static int realtime;
static int framenum;

// Input
typedef struct input_s
{
	int mousepos[2];
	int moused[2];
	bool lbuttondown;
	bool rbuttondown;
	bool keys[256];

} input_t;

static int mousepos[2];
static input_t input;


// ==============================================
// timing

static unsigned int GetMilliseconds()
{
	struct timeval	tp;

	gettimeofday(&tp, NULL);

	return (tp.tv_sec * 1000) + (tp.tv_usec / 1000); 
}

unsigned int Sys_Milliseconds (void)
{
	static unsigned int basetime;
	static unsigned int curtime;

	if (!basetime)
	{
		basetime = GetMilliseconds();
	}

	curtime = GetMilliseconds() - basetime;

	return curtime;
}

void Sys_Sleep(unsigned int msecs)
{
	usleep(msecs * 1000);
}

// ==============================================
// memory allocation

#define MEM_ALLOC_SIZE	32 * 1024 * 1024

typedef struct memstack_s
{
	unsigned char mem[MEM_ALLOC_SIZE];
	int allocated;

} memstack_t;

static memstack_t memstack;

void *Mem_Alloc(int numbytes)
{
	unsigned char *mem;
	
	if(memstack.allocated + numbytes > MEM_ALLOC_SIZE)
	{
		printf("Error: Mem: no free space available\n");
		abort();
	}

	mem = memstack.mem + memstack.allocated;
	memstack.allocated += numbytes;

	return mem;
}

void Mem_FreeStack()
{
	memstack.allocated = 0;
}

// ==============================================
// errors and warnings

static void Error(const char *error, ...)
{
	va_list valist;
	char buffer[2048];

	va_start(valist, error);
	vsprintf(buffer, error, valist);
	va_end(valist);

	printf("Error: %s", buffer);
	exit(1);
}

static void Warning(const char *warning, ...)
{
	va_list valist;
	char buffer[2048];

	va_start(valist, warning);
	vsprintf(buffer, warning, valist);
	va_end(valist);

	fprintf(stdout, "Warning: %s", buffer);
}

// ==============================================
// Misc crap

static float Vector_Dot(float a[3], float b[3])
{
	return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

static void Vector_Copy(float *a, float *b)
{
	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
}

static void Vector_Cross(float *c, float *a, float *b)
{
	c[0] = (a[1] * b[2]) - (a[2] * b[1]); 
	c[1] = (a[2] * b[0]) - (a[0] * b[2]); 
	c[2] = (a[0] * b[1]) - (a[1] * b[0]);
}

static void Vector_Normalize(float *v)
{
	float len, invlen;

	len = sqrtf((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
	invlen = 1.0f / len;

	v[0] *= invlen;
	v[1] *= invlen;
	v[2] *= invlen;
}

static void Vector_Lerp(float *result, float *from, float *to, float t)
{
	result[0] = ((1 - t) * from[0]) + (t * to[0]);
	result[1] = ((1 - t) * from[1]) + (t * to[1]);
	result[2] = ((1 - t) * from[2]) + (t * to[2]);
}

static void MatrixTranspose(float out[4][4], const float in[4][4])
{
	for( int i = 0; i < 4; i++ )
	{
		for( int j = 0; j < 4; j++ )
		{
			out[j][i] = in[i][j];
		}
	}
}

// ==============================================
// MD5 model

typedef struct md5joint_s
{
	char	*name;
	int		parentindex;
	int		flags;
	float	p[3];
	float	q[4];

} md5joint_t;

typedef struct md5vertex_s
{
	float	texcoords[2];
	int		firstweight;
	int		numweights;

} md5vertex_t;

typedef struct md5tri_s
{
	int		indicies[3];

} md5tri_t;

typedef struct md5weight_s
{
	int	joint;
	float	weight;
	float	xyz[3];

} md5weight_t;

typedef struct md5mesh_s
{
	struct md5mesh_s	*next;

	int				numvertices;
	md5vertex_t		*vertices;
	int				numtris;
	md5tri_t		*tris;
	int				numweights;
	md5weight_t		*weights;

} md5mesh_t;

typedef struct md5bound_s
{
	float			min[3];
	float			max[3];

} md5bound_t;

typedef struct md5animframe_s
{
	float		*data;
} md5animframe_t;

typedef struct md5anim_s
{
	struct md5anim_s	*next;

	char			*name;
	int				numframes;
	int				framerate;
	int				numanimatedcomponents;
	int				numjoints;
	
	md5joint_t		*joints;
	md5bound_t		*bounds;
	md5animframe_t	*frames;
	float			*framedata;

} md5anim_t;

typedef struct md5model_s
{
	int				numjoints;
	md5joint_t		*joints;
	int				nummeshes;
	md5mesh_t		*meshes;
	md5anim_t		*anims;

} md5model_t;

typedef struct md5jointmat_s
{
	float		m[3][4];

} md5jointmat_t;

#define MD5_ANIM_TX		(1 << 0)
#define MD5_ANIM_TY		(1 << 1)
#define MD5_ANIM_TZ		(1 << 2)

#define MD5_ANIM_QX		(1 << 3)
#define MD5_ANIM_QY		(1 << 4)
#define MD5_ANIM_QZ		(1 << 5)

static md5model_t* md5model;

char *Mem_AllocString(char *string)
{
	char *buffer = (char*)Mem_Alloc(strlen(string) + 1);
	strcpy(buffer, string);

	return buffer;
}

static md5joint_t *Mem_AllocMD5Joint(int numjoints)
{
	return (md5joint_t*)Mem_Alloc(numjoints * sizeof(md5joint_t));
}

static md5vertex_t *Mem_AllocMD5Vertex(int numvertices)
{
	return (md5vertex_t*)Mem_Alloc(numvertices * sizeof(md5vertex_t));
}

static md5tri_t *Mem_AllocMD5Tri(int numtris)
{
	return (md5tri_t*)Mem_Alloc(numtris * sizeof(md5tri_t));
}

static md5weight_t *Mem_AllocMD5Weight(int numweights)
{
	return (md5weight_t*)Mem_Alloc(numweights * sizeof(md5weight_t));
}

static md5mesh_t *Mem_AllocMD5Mesh(int nummeshes)
{
	return (md5mesh_t*)Mem_Alloc(nummeshes * sizeof(md5mesh_t));
}

static md5bound_t *Mem_AllocMD5Bound(int numbounds)
{
	return (md5bound_t*)Mem_Alloc(numbounds * sizeof(md5bound_t));
}

static md5animframe_t *Mem_AllocMD5AnimFrame(int numanimframes)
{
	return (md5animframe_t*)Mem_Alloc(numanimframes * sizeof(md5animframe_t));
}

static md5anim_t *Mem_AllocMD5Anim(int numanims)
{
	return (md5anim_t*)Mem_Alloc(numanims * sizeof(md5anim_t));
}

// model loading
static char *meshfilename;
static char *animfilename;

static float UnsignedIntToFloat(unsigned int u)
{
	union uf_t
	{
		unsigned int	u;
		float			f;
	} uf;
	
	uf.u = u;
	return uf.f;
}

static unsigned int ReadUnsignedInt(FILE *fp)
{
	unsigned int temp;
	fscanf(fp, "%x", &temp);

	return temp;
}

// returns a pointer to a static
static char* ReadToken(FILE *fp)
{
	static char buffer[1024];

	fscanf(fp, "%s", buffer);
	return buffer;
}

static void Eatline(FILE *fp)
{
	while(!feof(fp))
	{
		if(fgetc(fp) == '\n')
			break;
	}
}

static char* StripQuotes(char *string)
{
	char *src = string;
	char *dst = string;

	while(*src)
	{
		if(*src == '\"')
		{
			src++;
			continue;
		}

		*dst++ = *src++;
	}

	*dst = '\0';

	return string;
}

static char* ReadQuotedString(FILE *fp)
{
	static char buffer[1024];
	fscanf(fp, "%s", buffer);

	StripQuotes(buffer);

	return buffer;
}

static int ReadInt(FILE *fp)
{
	int i = atoi(ReadToken(fp));
	return i;
}

static float ReadFloat(FILE *fp)
{
	float f = (float)atof(ReadToken(fp));
	return f;
}

// ======================================================================================
// Mesh files

static float ComputeQuatW(md5joint_t *j)
{
	//float t = 1.0f - (j->q[0] * j->q[0]) - (j->q[1] * j->q[1]) - (j->q[2] * j->q[2]);
	//return (t < 0 ? 0.0 : -sqrtf(t));

	return -sqrtf( fabs( 1.0f - (j->q[0] * j->q[0] + j->q[1] * j->q[1] + j->q[2] * j->q[2])));
}

static void ReadJoint(FILE * fp, md5joint_t *j)
{
	j->name = Mem_AllocString(ReadQuotedString(fp));
	j->parentindex = ReadInt(fp);
	ReadToken(fp); // (
	j->p[0] = ReadFloat(fp);
	j->p[1] = ReadFloat(fp);
	j->p[2] = ReadFloat(fp);
	ReadToken(fp);	// )
	ReadToken(fp);	// (
	j->q[0] = ReadFloat(fp);
	j->q[1] = ReadFloat(fp);
	j->q[2] = ReadFloat(fp);
	j->q[3] = ComputeQuatW(j);
	ReadToken(fp);	//
	
	// eat the rest of the line
	Eatline(fp);
}

static void ReadJoints(FILE *fp)
{
	ReadToken(fp);	//	{

	for(int i = 0; i < md5model->numjoints; i++)
	{
		md5joint_t *j = md5model->joints + i;

		j->name = Mem_AllocString(ReadQuotedString(fp));
		j->parentindex = ReadInt(fp);
		ReadToken(fp); // (
		j->p[0] = ReadFloat(fp);
		j->p[1] = ReadFloat(fp);
		j->p[2] = ReadFloat(fp);
		ReadToken(fp);	// )
		ReadToken(fp);	// (
		j->q[0] = ReadFloat(fp);
		j->q[1] = ReadFloat(fp);
		j->q[2] = ReadFloat(fp);
		j->q[3] = ComputeQuatW(j);
		ReadToken(fp);	// )
		
		// eat the rest of the line
		Eatline(fp);
	}

	ReadToken(fp);	// }
}

static void ReadVertex(FILE *fp, md5vertex_t *v)
{
	v->texcoords[0]		= ReadFloat(fp);
	v->texcoords[1]		= ReadFloat(fp);
	v->firstweight		= ReadInt(fp);
	v->numweights		= ReadInt(fp);
}

static void ReadTri(FILE *fp, md5tri_t *t)
{
	t->indicies[0]		= ReadInt(fp);
	t->indicies[1]		= ReadInt(fp);
	t->indicies[2]		= ReadInt(fp);
}

static void ReadWeight(FILE *fp, md5weight_t *w)
{
	w->joint		= ReadInt(fp);
	w->weight		= ReadFloat(fp);
	ReadToken(fp);
	w->xyz[0]		= ReadFloat(fp);
	w->xyz[1]		= ReadFloat(fp);
	w->xyz[2]		= ReadFloat(fp);
	ReadToken(fp);
}

static void ReadMesh(FILE* fp)
{
	md5mesh_t *md5mesh = Mem_AllocMD5Mesh(1);

	// link the mesh into the list
	md5mesh->next = md5model->meshes;
	md5model->meshes = md5mesh;

	while(1)
	{
		char *token = ReadToken(fp);

		if(!strcmp(token, "numverts"))
		{
			md5mesh->numvertices = ReadInt(fp);
			md5mesh->vertices = Mem_AllocMD5Vertex(md5mesh->numvertices);
		}
		else if(!strcmp(token, "numtris"))
		{
			md5mesh->numtris = ReadInt(fp);
			md5mesh->tris = Mem_AllocMD5Tri(md5mesh->numtris);
		}
		else if(!strcmp(token, "numweights"))
		{
			md5mesh->numweights = ReadInt(fp);
			md5mesh->weights = Mem_AllocMD5Weight(md5mesh->numweights);
		}
		else if(!strcmp(token, "vert"))
		{
			int i = ReadInt(fp);
			md5vertex_t *v = md5mesh->vertices + i;

			ReadToken(fp);
			v->texcoords[0]		= ReadFloat(fp);
			v->texcoords[1]		= ReadFloat(fp);
			ReadToken(fp);
			v->firstweight		= ReadInt(fp);
			v->numweights		= ReadInt(fp);
		}
		else if(!strcmp(token, "tri"))
		{
			int i = ReadInt(fp);
			md5tri_t *t = md5mesh->tris + i;

			t->indicies[0]		= ReadInt(fp);
			t->indicies[1]		= ReadInt(fp);
			t->indicies[2]		= ReadInt(fp);
		}
		else if(!strcmp(token, "weight"))
		{
			int i = ReadInt(fp);
			md5weight_t *w = md5mesh->weights + i;

			w->joint		= ReadInt(fp);
			w->weight		= ReadFloat(fp);
			ReadToken(fp);
			w->xyz[0]		= ReadFloat(fp);
			w->xyz[1]		= ReadFloat(fp);
			w->xyz[2]		= ReadFloat(fp);
			ReadToken(fp);
		}
		else if(!strcmp(token, "}"))
		{
			break;
		}
	}
}

static void ReadMD5Model()
{
	FILE *fp = fopen(meshfilename, "r");
	if(!fp)
	{
		Error("couldn't open file %s\n", meshfilename);
	}

	while(!feof(fp))
	{
		char *token = ReadToken(fp);

		if(!strcmp(token, "numJoints"))
		{
			md5model->numjoints = ReadInt(fp);
			md5model->joints = Mem_AllocMD5Joint(md5model->numjoints);
		}
		if(!strcmp(token, "numMeshes"))
		{
			//md5model->nummeshes = ReadInt(fp);
			//md5model->meshes = Mem_AllocMD5Mesh(md5model->nummeshes);
		}
		if(!strcmp(token, "joints"))
		{
			ReadJoints(fp);
		}
		else if(!strcmp(token, "mesh"))
		{
			ReadMesh(fp);
		}
	}
}

// ======================================================================================
// Animation files

static void ReadHierarchy(FILE *fp, md5anim_t *md5anim)
{
	ReadToken(fp);

	for(int i = 0; i < md5anim->numjoints; i++)
	{
		md5joint_t *j = md5anim->joints + i;

		j->name			= Mem_AllocString(ReadQuotedString(fp));
		j->parentindex	= ReadInt(fp);
		j->flags		= ReadInt(fp);

		// eat the rest of the line
		Eatline(fp);
	}
}

static void ReadBounds(FILE *fp, md5anim_t *md5anim)
{
	if(!md5anim->bounds)
	{
		md5anim->bounds = Mem_AllocMD5Bound(md5anim->numframes);
	}

	ReadToken(fp);

	for(int i = 0; i < md5anim->numframes; i++)
	{
		md5bound_t *b = md5anim->bounds + i;

		ReadToken(fp); // (
		b->min[0] = ReadFloat(fp);
		b->min[1] = ReadFloat(fp);
		b->min[2] = ReadFloat(fp);
		ReadToken(fp);	// )
		ReadToken(fp);	// (
		b->max[0] = ReadFloat(fp);
		b->max[1] = ReadFloat(fp);
		b->max[2] = ReadFloat(fp);
		ReadToken(fp);	// )
	}
}

static void ReadBaseframe(FILE *fp, md5anim_t *md5anim)
{
	ReadToken(fp);

	for(int i = 0; i < md5anim->numjoints; i++)
	{
		md5joint_t *j = md5anim->joints + i;

		ReadToken(fp); // (
		j->p[0] = ReadFloat(fp);
		j->p[1] = ReadFloat(fp);
		j->p[2] = ReadFloat(fp);
		ReadToken(fp);	// )
		ReadToken(fp);	// (
		j->q[0] = ReadFloat(fp);
		j->q[1] = ReadFloat(fp);
		j->q[2] = ReadFloat(fp);
		j->q[3] = ComputeQuatW(j);
		ReadToken(fp);	// )
	}
}

static void ReadFrame(FILE *fp, md5anim_t *md5anim)
{
	// allocate the framedata if it hasn't already been allocated
	if(!md5anim->framedata)
	{
		md5anim->framedata = (float*)Mem_Alloc(sizeof(float) * md5anim->numframes * md5anim->numanimatedcomponents);
	}

	int framenum = ReadInt(fp);
	
	// setup the frame data structure
	md5animframe_t *frame = md5anim->frames + framenum;
	frame->data = framenum * md5anim->numanimatedcomponents + md5anim->framedata;

	ReadToken(fp);

	for(int i = 0; i < md5anim->numanimatedcomponents; i++)
	{
		frame->data[i] = ReadFloat(fp);
	}
}

static void ReadMD5Anim()
{
	md5anim_t *md5anim = Mem_AllocMD5Anim(1);
	
	// link the new animation into the list
	md5anim->next = md5model->anims;
	md5model->anims = md5anim;

	// open the file
	FILE *fp = fopen(animfilename, "r");
	if(!fp)
	{
		Error("couldn't open file %s\n", animfilename);
	}

	md5anim->name = Mem_AllocString(animfilename);

	while(!feof(fp))
	{
		char *token = ReadToken(fp);

		if(!strcmp(token, "numFrames"))
		{
			md5anim->numframes = ReadInt(fp);
			md5anim->frames = Mem_AllocMD5AnimFrame(md5anim->numframes);
		}
		else if(!strcmp(token, "numJoints"))
		{
			md5anim->numjoints = ReadInt(fp);
			md5anim->joints = Mem_AllocMD5Joint(md5anim->numjoints);
		}
		else if(!strcmp(token, "frameRate"))
		{
			md5anim->framerate = ReadInt(fp);
		}
		else if(!strcmp(token, "numAnimatedComponents"))
		{
			md5anim->numanimatedcomponents = ReadInt(fp);
		}
		else if(!strcmp(token, "hierarchy"))
		{
			ReadHierarchy(fp, md5anim);
		}
		else if(!strcmp(token, "bounds"))
		{
			ReadBounds(fp, md5anim);
		}
		else if(!strcmp(token, "baseframe"))
		{
			ReadBaseframe(fp, md5anim);
		}
		else if(!strcmp(token, "frame"))
		{
			ReadFrame(fp, md5anim);
		}
	}
}

static void ProcessMD5Files(int argc, char **argv)
{
	for(int i = 1; i < argc; i++)
	{
		if(strstr(argv[i], ".md5mesh"))
		{
			printf("processing file %s...\n", argv[i]);
			
			meshfilename = argv[i];
			md5model = (md5model_t*)Mem_Alloc(sizeof(md5model_t));
			
			ReadMD5Model();
			
			continue;
		}
		if(strstr(argv[i], ".md5anim"))
		{
			printf("processing file %s...\n", argv[i]);

			animfilename = argv[i];

			ReadMD5Anim();
			
			continue;
		}
	}
}

// Model rendering
static md5jointmat_t	jointmatlocal[256];
static md5jointmat_t	jointmatglobal[256];

static void Quat_Copy(float *a, float *b)
{
	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
	a[3] = b[3];
}

static void Quat_Negate(float *a, float *b)
{
	a[0] = -b[0];
	a[1] = -b[1];
	a[2] = -b[2];
	a[3] = -b[3];
}

static float Quat_Dot(float *a, float *b)
{
	return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]) + (a[3] * b[3]);
}

static void Quat_Normalize(float *q)
{
	float len, invlen;

	len = sqrtf((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3]));
	invlen = 1.0f / len;

	q[0] *= invlen;
	q[1] *= invlen;
	q[2] *= invlen;
	q[3] *= invlen;
}

#if 1
static void Quat_NLerp(float* result, float *from, float *to, float t)
{
	float cosom = from[0] * to[0] + from[1] * to[1] + from[2] * to[2] + from[3] * to[3];
	
	float scale0 = 1.0f - t;
	float scale1 = ( cosom > 0.0f ) ? t : -t;
	
	result[0] = scale0 * from[0] + scale1 * to[0];
	result[1] = scale0 * from[1] + scale1 * to[1];
	result[2] = scale0 * from[2] + scale1 * to[2];
	result[3] = scale0 * from[3] + scale1 * to[3];
	
	float s = 1.0f / sqrtf( result[0] * result[0] + result[1] * result[1] + result[2] * result[2] + result[3] * result[3] );
	
	result[0] *= s;
	result[1] *= s;
	result[2] *= s;
	result[3] *= s;
}
#endif

#if 0
static void Quat_NLerp(float* result, float *from, float *to, float t)
{
	float dot = Quat_Dot(from, to);
	if(dot < 0.0f)
	{
		Quat_Negate(to, to);
	}

	result[0] = ((1 - t) * from[0]) + (t * to[0]);
	result[1] = ((1 - t) * from[1]) + (t * to[1]);
	result[2] = ((1 - t) * from[2]) + (t * to[2]);
	result[3] = ((1 - t) * from[3]) + (t * to[3]);

	Quat_Normalize(result);
}
#endif

#if 0
/*
=====================
idQuat::Slerp

Spherical linear interpolation between two quaternions.
=====================
*/
idQuat &idQuat::Slerp( float *result, float *from, float *to, float t ) {
	idQuat	temp;
	float	omega, cosom, sinom, scale0, scale1;

	/*
	if(t <= 0.0f)
	{
		Quat_Copy(result, from);
		return;
	}

	if(t >= 1.0f)
	{
		Quat_Copy(result, to);
		return;
	}

	if(from == to)
	{
		*this = to;
		return *this;
	}
*/

	cosom = Quat_Dot(from, to);
	if(cosom < 0.0f)
	{
		Quat_Negate(temp, to);
		cosom = -cosom;
	}
	else
	{
		Quat_Copy(temp, to);
	}

	if((1.0f - cosom) > 1e-6f)
	{
#if 0
		omega = acos( cosom );
		sinom = 1.0f / sin( omega );
		scale0 = sin( ( 1.0f - t ) * omega ) * sinom;
		scale1 = sin( t * omega ) * sinom;
#else
		scale0 = 1.0f - cosom * cosom;
		sinom = idMath::InvSqrt( scale0 );
		omega = idMath::ATan16( scale0 * sinom, cosom );
		scale0 = idMath::Sin16( ( 1.0f - t ) * omega ) * sinom;
		scale1 = idMath::Sin16( t * omega ) * sinom;
#endif
	} else {
		scale0 = 1.0f - t;
		scale1 = t;
	}

	*this = ( scale0 * from ) + ( scale1 * temp );
	return *this;
}
#endif

static void JointMakeIdentity(md5jointmat_t *m)
{
	m->m[0][0] = 1.0f; m->m[0][1] = 0.0f; m->m[0][2] = 0.0f; m->m[0][3] = 0.0f;
	m->m[1][0] = 0.0f; m->m[1][1] = 1.0f; m->m[1][2] = 0.0f; m->m[1][3] = 0.0f;
	m->m[2][0] = 0.0f; m->m[2][1] = 0.0f; m->m[2][2] = 1.0f; m->m[2][3] = 0.0f;
}

#if 0
static void JointToMatrix(md5jointmat_t *m, md5joint_t *j)
{
	float	wx, wy, wz;
	float	xx, yy, yz;
	float	xy, xz, zz;
	float	x2, y2, z2;

	x2 = j->q[0] + j->q[0];
	y2 = j->q[1] + j->q[1];
	z2 = j->q[2] + j->q[2];

	xx = j->q[0] * x2;
	xy = j->q[0] * y2;
	xz = j->q[0] * z2;

	yy = j->q[1] * y2;
	yz = j->q[1] * z2;
	zz = j->q[2] * z2;

	wx = j->q[3] * x2;
	wy = j->q[3] * y2;
	wz = j->q[3] * z2;

	m->m[0][0] = 1.0f - (yy + zz);
	m->m[0][1] = (xy - wz);
	m->m[0][2] = (xz + wy);
	m->m[0][3] = j->p[0];

	m->m[1][0] = (xy + wz);
	m->m[1][1] = 1.0f - (xx + zz);
	m->m[1][2] = (yz - wx);
	m->m[1][3] = j->p[1];

	m->m[2][0] = (xz - wy);
	m->m[2][1] = (yz + wx);
	m->m[2][2] = 1.0f - (xx + yy);
	m->m[2][3] = j->p[2];
}
#endif

#if 1
static void JointToMatrix(md5jointmat_t *m, md5joint_t *j)
{
	float xx, xy, xz, xw;
	float yy, yz, yw;
	float zz, zw;

	xx = j->q[0] * j->q[0];
	xy = j->q[0] * j->q[1];
	xz = j->q[0] * j->q[2];
	xw = j->q[0] * j->q[3];

	yy = j->q[1] * j->q[1];
	yz = j->q[1] * j->q[2];
	yw = j->q[1] * j->q[3];

	zz = j->q[2] * j->q[2];
	zw = j->q[2] * j->q[3];

	m->m[0][0] = 1.0f - 2.0f * (yy + zz);
	m->m[0][1] = 2.0f * (xy - zw);
	m->m[0][2] = 2.0f * (xz + yw);
	m->m[0][3] = j->p[0];

	m->m[1][0] = 2.0f * (xy + zw);
	m->m[1][1] = 1.0f - 2.0f * (xx + zz);
	m->m[1][2] = 2.0f * (yz - xw);
	m->m[1][3] = j->p[1];

	m->m[2][0] = 2.0f * (xz - yw);
	m->m[2][1] = 2.0f * (yz + xw);
	m->m[2][2] = 1.0f - 2.0f * (xx + yy);
	m->m[2][3] = j->p[2];
}
#endif

static void JointMatrixMul(md5jointmat_t *c, md5jointmat_t *a, md5jointmat_t *b)
{
	c->m[0][0] = a->m[0][0] * b->m[0][0] + a->m[0][1] * b->m[1][0] + a->m[0][2] * b->m[2][0];
	c->m[0][1] = a->m[0][0] * b->m[0][1] + a->m[0][1] * b->m[1][1] + a->m[0][2] * b->m[2][1];
	c->m[0][2] = a->m[0][0] * b->m[0][2] + a->m[0][1] * b->m[1][2] + a->m[0][2] * b->m[2][2];
	c->m[0][3] = a->m[0][0] * b->m[0][3] + a->m[0][1] * b->m[1][3] + a->m[0][2] * b->m[2][3] + a->m[0][3];

	c->m[1][0] = a->m[1][0] * b->m[0][0] + a->m[1][1] * b->m[1][0] + a->m[1][2] * b->m[2][0];
	c->m[1][1] = a->m[1][0] * b->m[0][1] + a->m[1][1] * b->m[1][1] + a->m[1][2] * b->m[2][1];
	c->m[1][2] = a->m[1][0] * b->m[0][2] + a->m[1][1] * b->m[1][2] + a->m[1][2] * b->m[2][2];
	c->m[1][3] = a->m[1][0] * b->m[0][3] + a->m[1][1] * b->m[1][3] + a->m[1][2] * b->m[2][3] + a->m[1][3];

	c->m[2][0] = a->m[2][0] * b->m[0][0] + a->m[2][1] * b->m[1][0] + a->m[2][2] * b->m[2][0];
	c->m[2][1] = a->m[2][0] * b->m[0][1] + a->m[2][1] * b->m[1][1] + a->m[2][2] * b->m[2][1];
	c->m[2][2] = a->m[2][0] * b->m[0][2] + a->m[2][1] * b->m[1][2] + a->m[2][2] * b->m[2][2];
	c->m[2][3] = a->m[2][0] * b->m[0][3] + a->m[2][1] * b->m[1][3] + a->m[2][2] * b->m[2][3] + a->m[2][3];
}

static void JointVertexMul(float *c, md5jointmat_t *m, float *v)
{
	c[0] = m->m[0][0] * v[0] + m->m[0][1] * v[1] + m->m[0][2] * v[2] + m->m[0][3];
	c[1] = m->m[1][0] * v[0] + m->m[1][1] * v[1] + m->m[1][2] * v[2] + m->m[1][3];
	c[2] = m->m[2][0] * v[0] + m->m[2][1] * v[1] + m->m[2][2] * v[2] + m->m[2][3];
}

static void PrintMatrix(md5jointmat_t *m)
{
	printf("\n");
	printf("%3.6f, %3.6f, %3.6f, %3.6f\n", m->m[0][0], m->m[0][1], m->m[0][2], m->m[0][3]);
	printf("%3.6f, %3.6f, %3.6f, %3.6f\n", m->m[1][0], m->m[1][1], m->m[1][2], m->m[1][3]);
	printf("%3.6f, %3.6f, %3.6f, %3.6f\n", m->m[2][0], m->m[2][1], m->m[2][2], m->m[2][3]);
}

static void PrintJoint(md5joint_t *j)
{
	printf("joint (%3.6f, %3.6f, %3.6f) (%3.6f, %3.6f, %3.6f %3.6f)\n",
		j->p[0], j->p[1], j->p[2],
		j->q[0], j->q[1], j->q[2], j->q[3]);
}

static void ComputeGlobalMatrix(md5jointmat_t *m, int jointindex, md5joint_t *joints)
{
	JointMakeIdentity(m);

	while(jointindex != -1)
	{
		md5jointmat_t temp, localjointmat;

		JointToMatrix(&localjointmat, &joints[jointindex]);

		// take the joint and right multiply into the result
		JointMatrixMul(&temp, &localjointmat, m);
		*m = temp;	// fixme: take local copies so the original can be overwritten
		
		// get the next joint in the heirarchy
		jointindex = joints[jointindex].parentindex;
	}
}

static void ComputeLocalMatrices(md5jointmat_t *matrices, md5joint_t *joints, int numjoints)
{
	for(int i = 0; i < numjoints; i++)
	{
		JointToMatrix(&matrices[i], &joints[i]);
	}
}

static void ComputeGlobalMatrices(md5jointmat_t *matrices, md5joint_t *joints, int numjoints)
{
	for(int i = 0; i < numjoints; i++)
	{
		ComputeGlobalMatrix(&matrices[i], i, joints);
	}
}

static void ComputeFrameJoints(md5joint_t *joints, md5anim_t *anim, int frame)
{
	md5animframe_t *animframe = &anim->frames[frame];

	float *framedata = animframe->data;

	for(int i = 0; i < anim->numjoints; i++)
	{
		md5joint_t *j = &joints[i];
		
		// copy the base frame joint data
		*j = anim->joints[i];

		// copy the baseframe values
		j->name			= anim->joints[i].name;
		j->parentindex	= anim->joints[i].parentindex;
		j->flags		= anim->joints[i].flags;
		j->p[0]			= anim->joints[i].p[0];
		j->p[1]			= anim->joints[i].p[1];
		j->p[2]			= anim->joints[i].p[2];
		j->q[0]			= anim->joints[i].q[0];
		j->q[1]			= anim->joints[i].q[1];
		j->q[2]			= anim->joints[i].q[2];

		// modify the baseframes values with the frame values
		if(j->flags & MD5_ANIM_TX)
		{
			j->p[0] = *framedata;
			framedata++;
		}
		if(j->flags & MD5_ANIM_TY)
		{
			j->p[1] = *framedata;
			framedata++;
		}
		if(j->flags & MD5_ANIM_TZ)
		{
			j->p[2] = *framedata;
			framedata++;
		}
		if(j->flags & MD5_ANIM_QX)
		{
			j->q[0] = *framedata;
			framedata++;
		}
		if(j->flags & MD5_ANIM_QY)
		{
			j->q[1] = *framedata;
			framedata++;
		}
		if(j->flags & MD5_ANIM_QZ)
		{
			j->q[2] = *framedata;
			framedata++;
		}

		j->q[3] = ComputeQuatW(j);

		// don't offset the base joint
		if(i == 0)
		{
			j->p[0] = j->p[1] = j->p[2]= 0.0f;
		}
	}
}

static void LerpJoints(md5joint_t* result, md5joint_t* from, md5joint_t *to, float t, int numjoints)
{
	for(int i = 0; i < numjoints; i++)
	{
		// sigh...
		result[i].parentindex = from[i].parentindex;

		Vector_Lerp(result[i].p, from[i].p, to[i].p, t);
		Quat_NLerp(result[i].q, from[i].q, to[i].q, t);
	}
}

static void PrintJointList(md5joint_t *joints, int numjoints)
{
	for(int i = 0; i < numjoints; i++)
	{
		if(i != 62)
			continue;
		PrintJoint(&joints[i]);
	}
}

static void PrintMatrixList(md5jointmat_t *matrices, int nummatrices)
{
	for(int i = 0; i < nummatrices; i++)
	{
		if(i != 62)
			continue;

		PrintMatrix(&matrices[i]);
	}
}

static void DrawVector(float *origin, float *dir)
{
	float s = 2.0f;

	glBegin(GL_LINES);
	{
		glVertex3f(origin[0], origin[1], origin[2]);
		glVertex3f(s * dir[0] + origin[0], s * dir[1] + origin[1], s * dir[2] + origin[2]);
	}
	glEnd();
}

static void DrawMatrix(md5jointmat_t *matrix)
{
	float vectors[4][3] =
	{
		{ matrix->m[0][0], matrix->m[1][0], matrix->m[2][0] },
		{ matrix->m[0][1], matrix->m[1][1], matrix->m[2][1] },
		{ matrix->m[0][2], matrix->m[1][2], matrix->m[2][2] },
		{ matrix->m[0][3], matrix->m[1][3], matrix->m[2][3] },
	};

	glColor3f(1, 0, 0);
	DrawVector(vectors[3], vectors[0]);
	glColor3f(0, 1, 0);
	DrawVector(vectors[3], vectors[1]);
	glColor3f(0, 0, 1);
	DrawVector(vectors[3], vectors[2]);
}

static void RenderHierarchy(md5joint_t *joints, int numjoints)
{
	// draw the vectors
	{
		for(int i = 0; i < numjoints; i++)
		{
			md5jointmat_t jointmat;

			//JointToMatrix(&jointmat, &joints[i]);
			ComputeGlobalMatrix(&jointmat, i, joints);

			DrawMatrix(&jointmat);
		}
	}

	// draw the skeleton
	{
		for(int i = 0; i < numjoints; i++)
		{
			if(joints[i].parentindex == -1)
				continue;
			if(i != 62)
				continue;

			md5jointmat_t jointmat;
			//JointToMatrix(&jointmat, &joints[i]);
			ComputeGlobalMatrix(&jointmat, i, joints);

			md5jointmat_t parentmat;
			//JointToMatrix(&parentmat, &joints[joints[i].parentindex]);
			ComputeGlobalMatrix(&parentmat, joints[i].parentindex, joints);

			glColor3f(1, 1, 1);
			glBegin(GL_LINES);
			{
				glVertex3f(jointmat.m[0][3], jointmat.m[1][3], jointmat.m[2][3]);
				glVertex3f(parentmat.m[0][3], parentmat.m[1][3], parentmat.m[2][3]);
			}
			glEnd();
		}
	
	}
}

typedef struct drawvert_s
{
	float	xyz[3];
	float	normal[3];
	float	tangent[2][3];
	float	texcoord[2];
	float	color[3];

} drawvert_t;

typedef struct drawsurf_s
{
	drawvert_t		vertexbuffer[4096];
	int				numvertices;
	unsigned int	indexbuffer[8192];
	int				numindicies;

} drawsurf_t;

static drawsurf_t trisurf;

static void ComputeNormalsAndTangents(drawsurf_t *surf)
{
	int i;

	for(i = 0; i < surf->numvertices; i++)
	{
		drawvert_t *v = surf->vertexbuffer + i;

		v->normal[0] = v->normal[1] = v->normal[2] = 0.0f;
		v->tangent[0][0] = v->tangent[0][1] = v->tangent[0][2] = 0.0f;
		v->tangent[1][0] = v->tangent[1][1] = v->tangent[1][2] = 0.0f;
	}

	for(i = 0; i < surf->numindicies; i += 3)
	{
		// get the three vertices for the triangle
		drawvert_t *a = surf->vertexbuffer + surf->indexbuffer[i + 0];
		drawvert_t *b = surf->vertexbuffer + surf->indexbuffer[i + 1];
		drawvert_t *c = surf->vertexbuffer + surf->indexbuffer[i + 2];

		// compute direction vectors
		float d0[5];
		d0[0] = b->xyz[0] - a->xyz[0];
		d0[1] = b->xyz[1] - a->xyz[1];
		d0[2] = b->xyz[2] - a->xyz[2];
		d0[3] = b->texcoord[0] - a->texcoord[0];
		d0[4] = b->texcoord[1] - a->texcoord[1];
		
		float d1[5];
		d1[0] = c->xyz[0] - a->xyz[0];
		d1[1] = c->xyz[1] - a->xyz[1];
		d1[2] = c->xyz[2] - a->xyz[2];
		d1[3] = c->texcoord[0] - a->texcoord[0];
		d1[4] = c->texcoord[1] - a->texcoord[1];
		
		// calculate normal
		float normal[3];
		normal[0] = d1[1] * d0[2] - d1[2] * d0[1];
		normal[1] = d1[2] * d0[0] - d1[0] * d0[2];
		normal[2] = d1[0] * d0[1] - d1[1] * d0[0];

		const float f0 = 1.0f / sqrtf( normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2] );
		
		normal[0] *= f0;
		normal[1] *= f0;
		normal[2] *= f0;

		// texture area sign bit
		const float area = d0[3] * d1[4] - d0[4] * d1[3];
		unsigned int signbit = ( *( unsigned int* )&area ) & ( 1 << 31 );
	
		// calculate tangents
		float tangent[3];
		tangent[0] = d0[0] * d1[4] - d0[4] * d1[0];
		tangent[1] = d0[1] * d1[4] - d0[4] * d1[1];
		tangent[2] = d0[2] * d1[4] - d0[4] * d1[2];
		
		const float f1 = 1.0f / sqrtf( tangent[0] * tangent[0] + tangent[1] * tangent[1] + tangent[2] * tangent[2] );
		*( unsigned int* )&f1 ^= signbit;
		
		tangent[0] *= f1;
		tangent[1] *= f1;
		tangent[2] *= f1;
		
		float bitangent[3];
		bitangent[0] = d0[3] * d1[0] - d0[0] * d1[3];
		bitangent[1] = d0[3] * d1[1] - d0[1] * d1[3];
		bitangent[2] = d0[3] * d1[2] - d0[2] * d1[3];
		
		const float f2 = 1.0f / sqrtf( bitangent[0] * bitangent[0] + bitangent[1] * bitangent[1] + bitangent[2] * bitangent[2] );
		*( unsigned int* )&f2 ^= signbit;
		
		bitangent[0] *= f2;
		bitangent[1] *= f2;
		bitangent[2] *= f2;

		// add the normals and tangents to the vertices
		a->normal[0] += normal[0];
		a->normal[1] += normal[1];
		a->normal[2] += normal[2];
		a->tangent[0][0] += tangent[0];
		a->tangent[0][1] += tangent[1];
		a->tangent[0][2] += tangent[2];
		a->tangent[1][0] += bitangent[0];
		a->tangent[1][1] += bitangent[1];
		a->tangent[1][2] += bitangent[2];

		b->normal[0] += normal[0];
		b->normal[1] += normal[1];
		b->normal[2] += normal[2];
		b->tangent[0][0] += tangent[0];
		b->tangent[0][1] += tangent[1];
		b->tangent[0][2] += tangent[2];
		b->tangent[1][0] += bitangent[0];
		b->tangent[1][1] += bitangent[1];
		b->tangent[1][2] += bitangent[2];

		c->normal[0] += normal[0];
		c->normal[1] += normal[1];
		c->normal[2] += normal[2];
		c->tangent[0][0] += tangent[0];
		c->tangent[0][1] += tangent[1];
		c->tangent[0][2] += tangent[2];
		c->tangent[1][0] += bitangent[0];
		c->tangent[1][1] += bitangent[1];
		c->tangent[1][2] += bitangent[2];
	}

	// normalize the per-vertex normals and tangents
	for(i = 0; i < surf->numvertices; i++)
	{
		drawvert_t *v = surf->vertexbuffer + i;

		const float f0 = 1.0f / sqrtf( v->normal[0] * v->normal[0] + v->normal[1] * v->normal[1] + v->normal[2] * v->normal[2] );

		v->normal[0] *= f0;
		v->normal[1] *= f0;
		v->normal[2] *= f0;

		const float f1 = 1.0f / sqrtf( v->tangent[0][0] * v->tangent[0][0] + v->tangent[0][1] * v->tangent[0][1] + v->tangent[0][2] * v->tangent[0][2] );

		v->tangent[0][0] *= f1;
		v->tangent[0][1] *= f1;
		v->tangent[0][2] *= f1;

		const float f2 = 1.0f / sqrtf( v->tangent[1][0] * v->tangent[1][0] + v->tangent[1][1] * v->tangent[1][1] + v->tangent[1][2] * v->tangent[1][2] );

		v->tangent[1][0] *= f2;
		v->tangent[1][1] *= f2;
		v->tangent[1][2] *= f2;
	}
}

static void BuildIndexBuffer(drawsurf_t *surf, md5mesh_t *mesh)
{
	surf->numindicies = 0;
	unsigned int *indicies = surf->indexbuffer;
	
	for(int i = 0; i < mesh->numtris; i++)
	{
		indicies[0] = mesh->tris[i].indicies[0];
		indicies[1] = mesh->tris[i].indicies[1];
		indicies[2] = mesh->tris[i].indicies[2];
		indicies += 3;
		surf->numindicies += 3;
	}
}

static void ComputeVertexColors(drawsurf_t *surf)
{
	for(int i = 0; i < surf->numvertices; i++)
	{
		drawvert_t *v = surf->vertexbuffer + i;

		v->color[0] = 0.5f + 0.5f * v->normal[0];
		v->color[1] = 0.5f + 0.5f * v->normal[1];
		v->color[2] = 0.5f + 0.5f * v->normal[2];
	}
}

static void DrawNormals(drawsurf_t *surf)
{
	glBegin(GL_LINES);
	{
		for(int i = 0; i < surf->numvertices; i++)
		{
			drawvert_t *v = surf->vertexbuffer + i;
			
			glColor3f(1, 0, 0);
			DrawVector(v->xyz, v->tangent[0]);

			glColor3f(0, 1, 0);
			DrawVector(v->xyz, v->tangent[1]);

			glColor3f(0, 0, 1);
			DrawVector(v->xyz, v->normal);
		}
	}
	glEnd();
}


static void BuildVertexBuffer(drawsurf_t *surf, md5mesh_t *mesh, md5joint_t *joints)
{
	surf->numvertices = mesh->numvertices;

	for(int i = 0; i < mesh->numvertices; i++)
	{
		md5vertex_t *v = &mesh->vertices[i];

		// sum all the weights that affect this vertex
		// multiply the weights by the matrix
		float blendedvertex[3];
		blendedvertex[0] = blendedvertex[1] = blendedvertex[2] = 0.0f;
		for(int j = v->firstweight; j < v->firstweight + v->numweights; j++)
		{
			md5jointmat_t jointmat;
			ComputeGlobalMatrix(&jointmat, mesh->weights[j].joint, joints);

			float temp[3];
			JointVertexMul(temp, &jointmat, mesh->weights[j].xyz);

			// add this vertex to the total
			blendedvertex[0] += mesh->weights[j].weight * temp[0];
			blendedvertex[1] += mesh->weights[j].weight * temp[1];
			blendedvertex[2] += mesh->weights[j].weight * temp[2];
		}

		surf->vertexbuffer[i].xyz[0] = blendedvertex[0];
		surf->vertexbuffer[i].xyz[1] = blendedvertex[1];
		surf->vertexbuffer[i].xyz[2] = blendedvertex[2];
		surf->vertexbuffer[i].texcoord[0] = v->texcoords[0];
		surf->vertexbuffer[i].texcoord[1] = v->texcoords[1];
		//surf->vertexbuffer[i].color[0] = surf->vertexbuffer[i].texcoord[0];
		//surf->vertexbuffer[i].color[1] = surf->vertexbuffer[i].texcoord[1];
		//surf->vertexbuffer[i].color[2] = 0.0f;
		surf->vertexbuffer[i].color[0] = 0.5f + 0.5f * surf->vertexbuffer[i].normal[0];
		surf->vertexbuffer[i].color[1] = 0.5f + 0.5f * surf->vertexbuffer[i].normal[1];
		surf->vertexbuffer[i].color[2] = 0.5f + 0.5f * surf->vertexbuffer[i].normal[2];
	}
}

static void RenderGeometry(md5joint_t *joints)
{
	for(md5mesh_t *mesh = md5model->meshes; mesh; mesh = mesh->next)
	{
		BuildIndexBuffer(&trisurf, mesh);

		BuildVertexBuffer(&trisurf, mesh, joints);

		// These should be seperate to the other two
		ComputeNormalsAndTangents(&trisurf);
		ComputeVertexColors(&trisurf);

		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glVertexPointer(3, GL_FLOAT, sizeof(drawvert_t), trisurf.vertexbuffer->xyz);
		glColorPointer(3, GL_FLOAT, sizeof(drawvert_t), trisurf.vertexbuffer->color);
		glDrawElements(GL_TRIANGLES, trisurf.numindicies, GL_UNSIGNED_INT, trisurf.indexbuffer);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		DrawNormals(&trisurf);
	}
}

// frame joints and matrices
static md5joint_t	framejoints[256];
static md5jointmat_t	framemats[256];
static md5joint_t	framejoints2[2][256];
// static currentanim

// fixme: fill this out. all anim crap should go in here
typedef struct animstate_s
{

} animstate_t;

static void JointTest()
{
	//ComputeFrameJoints(framejoints, &md5model->anims[0], framenum % md5model->anims[0].numframes);
	//PrintJointList(framejoints, md5model->numjoints);
	float time = framenum * 0.016f;
	float animtime = time * 6; //md5model->anims[0].framerate;
	int frame0 = (int)floor(animtime) % md5model->anims[0].numframes;
	int frame1 = (frame0 + 1) % md5model->anims[0].numframes;
	float lerp = animtime - floor(animtime);

	//if(frame0 > md5model->anims[0].numframes)
	//	frame0 = md5model->anims[0].numframes - 1;
	//if(frame1 > md5model->anims[0].numframes)
	//	frame1 = md5model->anims[0].numframes - 1;
	//frame0 = framenum % md5model->anims[0].numframes;
	//frame1 = (framenum + 1) % md5model->anims[0].numframes;

	//if(frame0 == 17)
	//{
	//	frame0 = 17;
	//	frame1 = 19;
	//}
	//frame0 = 19; frame1 = 20;
	//lerp = 0.0f;
	
	ComputeFrameJoints(framejoints2[0], &md5model->anims[0], frame0);

	ComputeFrameJoints(framejoints2[1], &md5model->anims[0], frame1);

	LerpJoints(framejoints, framejoints2[0], framejoints2[1], lerp, md5model->anims[0].numjoints);

	printf("frame %i\n", frame0);
	PrintJointList(framejoints2[0], md5model->anims[0].numjoints);
	PrintJointList(framejoints2[1], md5model->anims[0].numjoints);
	PrintJointList(framejoints, md5model->anims[0].numjoints);

	RenderHierarchy(framejoints, md5model->numjoints);

	RenderGeometry(framejoints);
}

//==============================================
// simulation code

static float viewangles[2];
static float viewpos[3];
static float viewvectors[3][3];

typedef struct tickcmd_s
{
	float	forwardmove;
	float	sidemove;
	float	anglemove[2];

} tickcmd_t;

static tickcmd_t gcmd;

// This must be part of game state or else movement becomes dependent on the rendering
static void VectorsFromSphericalAngles(float vectors[3][3], float angles[2])
{
	float cx, sx, cy, sy, cz, sz;

	cx = 1.0f;
	sx = 0.0f;
	cy = cosf(angles[0]);
	sy = sinf(angles[0]);
	cz = cosf(angles[1]);
	sz = sinf(angles[1]);

	vectors[0][0] = cy * cz;
	vectors[0][1] = sz;
	vectors[0][2] = -sy * cz;

	vectors[1][0] = (-cx * cy * sz) + (sx * sy);
	vectors[1][1] = cx * cz;
	vectors[1][2] = (cx * sy * sz) + (sx * cy);

	vectors[2][0] = (sx * cy * sz) + (cx * sy);
	vectors[2][1] = (-sx * cz);
	vectors[2][2] = (-sx * sy * sz) + (cx * cy);
}

// build a current command from the input state
static void BuildTickCmd()
{
	tickcmd_t *cmd = &gcmd;
	float scale;
	
	// Move forward ~512 units each second (60 * 4.2)
	scale = 4.2f;

	cmd->forwardmove = 0.0f;
	cmd->sidemove = 0.0f;
	cmd->anglemove[0] = 0.0f;
	cmd->anglemove[1] = 0.0f;

	if(input.keys['w'])
	{
		cmd->forwardmove += scale;
	}

	if(input.keys['s'])
	{
		cmd->forwardmove -= scale;
	}

	if(input.keys['d'])
	{
		cmd->sidemove += scale;
	}

	if(input.keys['a'])
	{
		cmd->sidemove -= scale;
	}

	// Handle mouse movement
	if(input.lbuttondown)
	{
		cmd->anglemove[0] = -0.01f * (float)input.moused[0];
		cmd->anglemove[1] = -0.01f * (float)input.moused[1];
	}
}

// input capture
static bool		inputcapture = false;
static bool		inputplayback = false;
static char*	inputfilename = NULL;
static FILE*	fpinputcapture;
static int		numinputswritten;
static int	inputstartframe;
static int	inputendframe;

// fixme: how to setup output filename?
static void BeginInputCapture()
{
	fpinputcapture = fopen("input.bin", "wb");

	// capture initial state
	fwrite(viewpos, sizeof(float), 3, fpinputcapture);
	fwrite(viewangles, sizeof(float), 2, fpinputcapture);

	numinputswritten = 0;
}

static void EndInputCapture()
{
	fclose(fpinputcapture);

	printf("captured %d inputs\n", numinputswritten);
}

static void WriteInput()
{
	if(!inputcapture)
		return;

	fwrite(&gcmd, sizeof(tickcmd_t), 1, fpinputcapture);

	numinputswritten++;
}

// input playback
static void ReadNextInput()
{	
	if(!inputplayback)
		return;

	if(!fpinputcapture)
	{
		fpinputcapture = fopen(inputfilename, "rb");
		fread(viewpos, sizeof(float), 3, fpinputcapture);
		fread(viewangles, sizeof(float), 2, fpinputcapture);
		return;
	}

	fread(&gcmd, sizeof(tickcmd_t), 1, fpinputcapture);

	if(feof(fpinputcapture))
	{
		inputplayback = false;
		fclose(fpinputcapture);

		printf("finished input playback\n");
	}
}

// apply the tick command to the viewstate
static void DoMove()
{
	tickcmd_t *cmd = &gcmd;

	VectorsFromSphericalAngles(viewvectors, viewangles);

	viewpos[0] += cmd->forwardmove * viewvectors[0][0];
	viewpos[1] += cmd->forwardmove * viewvectors[0][1];
	viewpos[2] += cmd->forwardmove * viewvectors[0][2];

	viewpos[0] += cmd->sidemove * viewvectors[2][0];
	viewpos[1] += cmd->sidemove * viewvectors[2][1];
	viewpos[2] += cmd->sidemove * viewvectors[2][2];

	viewangles[0] += cmd->anglemove[0];
	viewangles[1] += cmd->anglemove[1];

	if(viewangles[1] >= PI / 2.0f)
		viewangles[1] = (PI / 2.0f) - 0.001f;
	if(viewangles[1] <= -PI/ 2.0f)
		viewangles[1] = (-PI / 2.0f) + 0.001f;
}

static void SetupDefaultViewPos()
{
	// look down negative z
	viewangles[0] = PI / 2.0f;
	viewangles[1] = 0.0f;
	
	viewpos[0] = 0.0f;
	viewpos[1] = 0.0f;
	viewpos[2] = 256.0f;
}

// Advance the state of everything by one frame
static void Ticker()
{
	BuildTickCmd();
	
	ReadNextInput();

	WriteInput();

	DoMove();
}

static void MainLoop()
{
	// initialize the base time
	if(!oldtime)
	{
		oldtime = Sys_Milliseconds();
	}

	int newtime = Sys_Milliseconds();
	int deltatime = newtime - oldtime;
	oldtime = newtime;

	// wait until some time has elapsed
	if(deltatime < 1)
	{
		Sys_Sleep(1);
		return;
	}
	
	// figure out how many tick s to run?
	// sync frames?
	if(deltatime > 50)
		deltatime = 0;

	// update realtime
	realtime += deltatime;

	// run a tick if enough time has elapsed
	// should realtime be clamped if we're dropping frames?
	//if(realtime > framenum * 16)
	while(realtime > framenum * 16)
	{
		framenum++;

		Ticker();
	}

	// signal a screen redraw (should this be sync'd with simulation updates
	// or free running?) glutPostRedisplay signals the draw callback to be called
	// on the next pass through the glutMainLoop
	// run as fast as possible to capture the mouse movements
	glutPostRedisplay();
}

//==============================================
// OpenGL rendering code
//
// this stuff touches some of the simulation state (viewvectors, viewpos etc)
// guess it should really have an interface to extract that data?

static int renderwidth;
static int renderheight;

static int rendermode = 0;
typedef void (*drawfunc_t)();

static void GL_LoadMatrix(float m[4][4])
{
	glLoadMatrixf((float*)m);
}

static void GL_LoadMatrixTranspose(float m[4][4])
{
	float t[4][4];

	MatrixTranspose(t, m);
	glLoadMatrixf((float*)t);
}

static void GL_MultMatrix(float m[4][4])
{
	glMultMatrixf((float*)m);
}

static void GL_MultMatrixTranspose(float m[4][4])
{
	float t[4][4];

	MatrixTranspose(t, m);
	glMultMatrixf((float*)t);
}

static void DrawAxis()
{
	glBegin(GL_LINES);
	glColor3f(1, 0, 0);
	glVertex3f(0, 0, 0);
	glVertex3f(32, 0, 0);

	glColor3f(0, 1, 0);
	glVertex3f(0, 0, 0);
	glVertex3f(0, 32, 0);

	glColor3f(0, 0, 1);
	glVertex3f(0, 0, 0);
	glVertex3f(0, 0, 32);
	glEnd();
}

static void SetModelViewMatrix()
{
	// matrix to transform from look down x to looking down -z
	static float yrotate[4][4] =
	{
		{ 0, 0, 1, 0 },
		{ 0, 1, 0, 0 },
		{ -1, 0, 0, 0 },
		{ 0, 0, 0, 1 }
	};

	// matrix to convert from doom coordinates to gl coordinates
	static float doomtogl[4][4] = 
	{
		{ 1, 0, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, -1, 0, 0 },
		{ 0, 0, 0, 1 }
	};

	float matrix[4][4];
	matrix[0][0]	= viewvectors[0][0];
	matrix[0][1]	= viewvectors[0][1];
	matrix[0][2]	= viewvectors[0][2];
	matrix[0][3]	= -(viewvectors[0][0] * viewpos[0]) - (viewvectors[0][1] * viewpos[1]) - (viewvectors[0][2] * viewpos[2]);

	matrix[1][0]	= viewvectors[1][0];
	matrix[1][1]	= viewvectors[1][1];
	matrix[1][2]	= viewvectors[1][2];
	matrix[1][3]	= -(viewvectors[1][0] * viewpos[0]) - (viewvectors[1][1] * viewpos[1]) - (viewvectors[1][2] * viewpos[2]);

	matrix[2][0]	= viewvectors[2][0];
	matrix[2][1]	= viewvectors[2][1];
	matrix[2][2]	= viewvectors[2][2];
	matrix[2][3]	= -(viewvectors[2][0] * viewpos[0]) - (viewvectors[2][1] * viewpos[1]) - (viewvectors[2][2] * viewpos[2]);

	matrix[3][0]	= 0.0f;
	matrix[3][1]	= 0.0f;
	matrix[3][2]	= 0.0f;
	matrix[3][3]	= 1.0f;

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	GL_MultMatrixTranspose(yrotate);
	GL_MultMatrixTranspose(matrix);
	GL_MultMatrixTranspose(doomtogl);
}

static void R_SetPerspectiveMatrix(float fov, float aspect, float znear, float zfar)
{
	float r, l, t, b;
	float fovx, fovy;
	float m[4][4];

	// fixme: move this somewhere else
	fovx = fov * (3.1415f / 360.0f);
	float x = (renderwidth / 2.0f) / atan(fovx);
	fovy = atan2(renderheight / 2.0f, x);

	// Calcuate right, left, top and bottom values
	r = znear * fovx; //tan(fovx * (3.1415f / 360.0f));
	l = -r;

	t = znear * fovy; //tan(fovy * (3.1415f / 360.0f));
	b = -t;

	m[0][0] = (2.0f * znear) / (r - l);
	m[1][0] = 0;
	m[2][0] = (r + l) / (r - l);
	m[3][0] = 0;

	m[0][1] = 0;
	m[1][1] = (2.0f * znear) / (t - b);
	m[2][1] = (t + b) / (t - b);
	m[3][1] = 0;

	m[0][2] = 0;
	m[1][2] = 0;
	m[2][2] = -(zfar + znear) / (zfar - znear);
	m[3][2] = -2.0f * zfar * znear / (zfar - znear);

	m[0][3] = 0;
	m[1][3] = 0;
	m[2][3] = -1;
	m[3][3] = 0;

	glMatrixMode(GL_PROJECTION);
	GL_LoadMatrix(m);
}

static void BeginFrame()
{
	glClearColor(0.3f, 0.3f, 0.3f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glFrontFace(GL_CW);
	glEnable(GL_CULL_FACE);
}

static void Draw()
{
	BeginFrame();

	SetModelViewMatrix();

	DrawAxis();

	JointTest();
}

//==============================================
// GLUT/OS/windowing code

// Called every frame to process the current mouse input state
// We only get updates when the mouse moves so the current mouse
// position is stored and may be used for multiple frames
static void ProcessInput()
{
	// mousepos has current "frame" mouse pos
	input.moused[0] = mousepos[0] - input.mousepos[0];
	input.moused[1] = mousepos[1] - input.mousepos[1];
	input.mousepos[0] = mousepos[0];
	input.mousepos[1] = mousepos[1];
}

static void DisplayFunc()
{
	Draw();

	//if(glGetError() != GL_NO_ERROR)
	//	Error("Got a GL error\n");
	//glFlush();
	//int time1 = Sys_Milliseconds();
	//printf("preswap: %i\n", time1);
	glutSwapBuffers();
	//int time2 = Sys_Milliseconds();
	//printf("pstswap: %i\n", time2);
}

static void KeyboardDownFunc(unsigned char key, int x, int y)
{
	input.keys[key] = true;

	if(key == 'q')
	{
		inputcapture = !inputcapture;

		if(inputcapture)
		{
			fprintf(stdout, "Begin input capture\n");
			BeginInputCapture();
		}
		else
		{
			fprintf(stdout, "End input capture\n");
			EndInputCapture();
		}
	}

	if(key == 'r')
	{
		rendermode++;
		if(rendermode == 2)
			rendermode = 0;
	}
}

static void KeyboardUpFunc(unsigned char key, int x, int y)
{
	input.keys[key] = false;
}

static void ReshapeFunc(int w, int h)
{
	renderwidth = w;
	renderheight = h;

	R_SetPerspectiveMatrix(90.0f, (float)w / (float)h, 3, 4096.0f);

	glViewport(0, 0, w, h);
}

static void MouseFunc(int button, int state, int x, int y)
{
	if(button == GLUT_LEFT_BUTTON)
		input.lbuttondown = (state == GLUT_DOWN);
	if(button == GLUT_RIGHT_BUTTON)
		input.rbuttondown = (state == GLUT_DOWN);
}

static void MouseMoveFunc(int x, int y)
{
	mousepos[0] = x;
	mousepos[1] = y;
}

static void MainLoopFunc()
{
	ProcessInput();

	MainLoop();
}

static void PrintUsage()
{}

static void ProcessCommandLine(int argc, char *argv[])
{
	int i;

	for(i = 1; i < argc; i++)
	{
		if(argv[i][0] != '-')
			break;

		if(!strcmp(argv[i], "--input"))
		{
			inputplayback = true;
			inputfilename = argv[i + i];
			i++;
		}
		else if(!strcmp(argv[i], "--start-frame"))
		{
			Error("--start-frame not implemented\n");
		}
		else if(!strcmp(argv[i], "--end-frame"))
		{
			Error("--end-frame not implemented\n");
		}
		else
		{
			Error("Unknown option %s\n", argv[i]);
		}
	}

	if(i == argc)
	{
		Error("No input file\n");
	}
}

int main(int argc, char *argv[])
{
	glutInit(&argc, argv);
	
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(400, 400);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);
	glutCreateWindow("test");

	ProcessCommandLine(argc, argv);

	ProcessMD5Files(argc, argv);
	
	SetupDefaultViewPos();

	glutReshapeFunc(ReshapeFunc);
	glutDisplayFunc(DisplayFunc);
	glutKeyboardFunc(KeyboardDownFunc);
	glutKeyboardUpFunc(KeyboardUpFunc);
	glutMouseFunc(MouseFunc);
	glutMotionFunc(MouseMoveFunc);
	glutPassiveMotionFunc(MouseMoveFunc);
	glutIdleFunc(MainLoopFunc);
	//glutIdleFunc(TimeDemoFunc);

	glutMainLoop();

	return 0;
}

