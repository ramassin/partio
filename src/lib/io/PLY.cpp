/*
AAAAAAAAAAAAAAAAAAAAAAAARRRRRRRRRRRRRRRRRRRRRRGHHHHHHHHHHHHHHHHHHHHHH
*/
#include "../Partio.h"
#include "../core/ParticleHeaders.h"
#include "ZIP.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cassert>
#include <memory>
#include <cstring>


#include <stdio.h> 
#include "../extern/rply.h"


Partio::ParticleAttribute idHandle, posHandle, colHandle, qualHandle;

// vertex position handler callback
static int vertex_pos_cb(p_ply_argument argument) {
	long idx;
	//unfortunately we have to keep counting the calls to set the right particle

	static unsigned int counter = 0; 
	void* pinfo = 0;
	ply_get_argument_user_data(argument, &pinfo, &idx);
	Partio::ParticlesDataMutable* particle =reinterpret_cast<Partio::ParticlesDataMutable *>(pinfo);

	int * id = particle->dataWrite<int>(idHandle,counter);
	*id=counter;

	float* pos=particle->dataWrite<float>(posHandle,counter);
	float val = (float)ply_get_argument_value(argument);
	pos[idx]=val;
	if (idx==2)
		counter++;
	//printf("vertexPos %d: %g", idx, val);
	//printf(" ");

	return 1;
}

// vertex color handler callback
static int vertex_col_cb(p_ply_argument argument) {
	long idx;
	static unsigned int counter = 0; //unfortunately we have to keep a local counter
	void* pinfo = 0;
	ply_get_argument_user_data(argument, &pinfo, &idx);
	Partio::ParticlesDataMutable* particle =reinterpret_cast<Partio::ParticlesDataMutable *>(pinfo);

	float* pos=particle->dataWrite<float>(colHandle,counter);
	float val = (float)ply_get_argument_value(argument)/255.0f;
	pos[idx]=val;
	if (idx==2)
		counter++;

	//printf("color %d: %g", idx, ply_get_argument_value(argument));
	//if (idx==2) printf("\n");
	//else printf(" ");
	return 1;
}

// vertex quality handler callback
static int vertex_qual_cb(p_ply_argument argument) {
	static unsigned int counter = 0; //unfortunately we have to keep a local counter
	void* pinfo = 0;
	ply_get_argument_user_data(argument, &pinfo, NULL);
	Partio::ParticlesDataMutable* particle =reinterpret_cast<Partio::ParticlesDataMutable *>(pinfo);

	float* pos=particle->dataWrite<float>(qualHandle,counter);
	float val = (float)ply_get_argument_value(argument);
	*pos=val;
	counter++;

	return 1;
}

static Partio::ParticleAttributeType typePLY2Partio(e_ply_type plyType){
	 Partio::ParticleAttributeType partioType;
	 /*    PLY_INT8, PLY_UINT8, PLY_INT16, PLY_UINT16, 
    PLY_INT32, PLY_UIN32, PLY_FLOAT32, PLY_FLOAT64,
    PLY_CHAR, PLY_UCHAR, PLY_SHORT, PLY_USHORT,
    PLY_INT, PLY_UINT, PLY_FLOAT, PLY_DOUBLE,
    PLY_LIST */
	 switch (plyType)
	 {
		case e_ply_type::PLY_INT8 :
		case e_ply_type::PLY_UINT8 :
		case e_ply_type::PLY_INT16 :
		case e_ply_type::PLY_UINT16 :
		case e_ply_type::PLY_INT32 :
		case e_ply_type::PLY_CHAR :
		case e_ply_type::PLY_UCHAR :
		case e_ply_type::PLY_SHORT :
		case e_ply_type::PLY_USHORT :
		case e_ply_type::PLY_INT :
		case e_ply_type::PLY_UINT :
			partioType = Partio::ParticleAttributeType::INT;
			break;

		case e_ply_type::PLY_FLOAT:
		case e_ply_type::PLY_FLOAT32:
		case e_ply_type::PLY_FLOAT64:
		case e_ply_type::PLY_DOUBLE:
			partioType = Partio::ParticleAttributeType::INT;
			break;

		default:
			partioType = Partio::ParticleAttributeType::NONE;
			break;
	 }
	
}

namespace Partio
{

	using namespace std;

	// TODO: convert this to use iterators like the rest of the readers/writers

	ParticlesDataMutable* readPLY(const char* filename,const bool headersOnly)
	{
		p_ply input = ply_open(filename, NULL, 0, NULL);

		if (!input)
		{
			cerr<<"Partio: Can't open particle data file: "<<filename<<endl;
			return 0;
		}

		if (!ply_read_header(input)) 
		{
			cerr<<"Partio: Problem parsing PLY header in file: "<<filename<<endl;
			return 0;
		}
		Partio::ParticlesDataMutable* simple = 0;
		if (headersOnly) simple=new ParticleHeaders;
		else simple=create();
		printf("created!\n");  

		// create null pointer to partio stuct for callbacks
		void * simplePtr = reinterpret_cast<void *>(simple);

		unsigned int nPart0 = ply_set_read_cb(input, "vertex", "x", vertex_pos_cb, simplePtr, 0);
		unsigned int nPart1 = ply_set_read_cb(input, "vertex", "y", vertex_pos_cb, simplePtr, 1);
		unsigned int nPart2 = ply_set_read_cb(input, "vertex", "z", vertex_pos_cb, simplePtr, 2);

		if (!(nPart0 == nPart1 && nPart0 == nPart2))	{ 
			cerr<<"Partio: Problem parsing PLY properties in file: "<<filename<<endl;
			return 0;
		}

		else {
			idHandle =	simple->addAttribute("id",  Partio::INT, 1);
			posHandle = simple->addAttribute("position",  VECTOR, 3);
		}

		unsigned int nCol0  = ply_set_read_cb(input, "vertex", "red",   vertex_col_cb, simplePtr, 0);
		unsigned int nCol1  = ply_set_read_cb(input, "vertex", "green", vertex_col_cb, simplePtr, 1);
		unsigned int nCol2  = ply_set_read_cb(input, "vertex", "blue",  vertex_col_cb, simplePtr, 2);

		const char* lastComment = NULL;

		//display comments
		while ((lastComment = ply_get_next_comment(input, lastComment)))
		{
			printf("[PLY][Comment] %s\n",lastComment);
		}

		p_ply_element vertexElem = ply_get_next_element(input, NULL);
		
		const char * elemName;
		long elemCount;
		ply_get_element_info(vertexElem, &elemName, &elemCount);
		printf("elem name %s count %i \n", elemName, elemCount);

		p_ply_property thisProp = NULL;

		std::vector<ParticleAttribute> partAttr;

		while (thisProp = ply_get_next_property(vertexElem, thisProp))
		{
			const char * propName;
			e_ply_type  propType;

			ply_get_property_info(thisProp, &propName, &propType, NULL, NULL);
			printf("%s: type %i \n", propName, propType);
			ParticleAttribute thisAttribute;
			thisAttribute.name=std::string(propName);
			
			thisAttribute.type=Partio::ParticleAttributeType::FLOAT;

		} 


		if (nCol0 == 0) 	{
			 cout<<"Partio: No colors in file: "<<filename<<endl;
		}
		else if ((nPart0 != nCol0) || !(nCol0 == nCol1  && nCol0 == nCol2))	{
			cerr<<"Partio: Problem with colours in file: "<<filename<<endl;
			return 0;
		}
		else {
			colHandle = simple->addAttribute("pointColor",  VECTOR, 3);	
		}

		unsigned int nQual = ply_set_read_cb(input, "vertex", "quality",  vertex_qual_cb, simplePtr, 0);

		if (nQual>0)
			qualHandle = simple->addAttribute("quality",  FLOAT, 1);

		simple->addParticles(nPart0);

		if (!ply_read(input)){
			cerr<<"Partio: Problem parsing PLY data in file: "<<filename<<endl;
			return 0;
		}

		ply_close(input);
		//printf("read!\n");
		return simple;
	}

}