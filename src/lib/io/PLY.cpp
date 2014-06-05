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
#include "../../extern/rply/rply.h"


// GLOBALS 
p_ply input;
Partio::ParticlesDataMutable* simple = 0;
void * simplePtr;

Partio::ParticleAttribute idHandle, posHandle, colHandle;
std::vector<Partio::ParticleAttribute> genericHandles;
std::string inputFileName;

std::map<std::string, e_ply_type> attrs;

// vertex position handler callback
static int vertex_pos_cb(p_ply_argument argument) {
	long idx;
	//unfortunately we have to keep counting the calls to set the right particle

	static unsigned int counter = 0; 
	void* pinfo = 0;
	ply_get_argument_user_data(argument, &pinfo, &idx);
	Partio::ParticlesDataMutable* particle =reinterpret_cast<Partio::ParticlesDataMutable *>(pinfo);

	int nParticles = particle->numParticles();
	if (counter > nParticles)
		counter = 0;

	int * id = particle->dataWrite<int>(idHandle,counter);
	*id=counter;

	float* pos=particle->dataWrite<float>(posHandle,counter);
	float val = (float)ply_get_argument_value(argument);
	pos[idx]=val;
	if (idx==2)
		counter++;
	//if (counter>170000) printf("vertex %d Pos  %d: %g\n", counter, idx, val);
	//printf(" ");

	return 1;
}

// vertex color handler callback for uints
static int vertex_col_cb_uint(p_ply_argument argument) {
	const float scaleFactor=1.0f/255.0f;
	long idx;

	static unsigned int counter = 0; //unfortunately we have to keep a local counter
	void* pinfo = 0;
	ply_get_argument_user_data(argument, &pinfo, &idx);
	Partio::ParticlesDataMutable* particle =reinterpret_cast<Partio::ParticlesDataMutable *>(pinfo);

	int nParticles = particle->numParticles();
	if (counter > nParticles)
		counter = 0;

	float* pos=particle->dataWrite<float>(colHandle,counter);
	float val = (float)ply_get_argument_value(argument) * scaleFactor;
	pos[idx]=val;

	// after we've done with Blue, increment particle index
	if (idx==2)
		counter++;

	//printf("color %d: %g", idx, ply_get_argument_value(argument));
	//if (idx==2) printf("\n");
	//else printf(" ");
	return 1;
}

// vertex color handler callback for floats
static int vertex_col_cb_float(p_ply_argument argument) {

	long idx;

	static unsigned int counter = 0; //unfortunately we have to keep a local counter
	void* pinfo = 0;
	ply_get_argument_user_data(argument, &pinfo, &idx);
	Partio::ParticlesDataMutable* particle =reinterpret_cast<Partio::ParticlesDataMutable *>(pinfo);

	if (counter > particle->numParticles())
		counter = 0;

	float* pos=particle->dataWrite<float>(colHandle,counter);
	float val = (float)ply_get_argument_value(argument);
	pos[idx]=val;

	// after we've done with Blue, increment particle index
	if (idx==2)
		counter++;

	//printf("color %d: %g", idx, ply_get_argument_value(argument));
	//if (idx==2) printf("\n");
	//else printf(" ");
	return 1;
}

// generic float callback, the integer value is now the index in the generic handle vector
static int generic_vertex_cb(p_ply_argument argument) {

	static std::vector<unsigned int> counters(32); //do you really need more than 32?
	long idx;
	void* pinfo = 0;
	ply_get_argument_user_data(argument, &pinfo, &idx);
	Partio::ParticlesDataMutable* particle =reinterpret_cast<Partio::ParticlesDataMutable *>(pinfo);

	if (counters[idx] > particle->numParticles())
		counters[idx] = 0;
	float* pos=particle->dataWrite<float>(genericHandles[idx],counters[idx]);
	float val = (float)ply_get_argument_value(argument);
	*pos=val;
	counters[idx]++;

	return 1;
}



namespace Partio
{

	using namespace std;

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
	return partioType;
}
//return a vector of properties from ply header
typedef std::pair<std::string, e_ply_type> plyVertexAttrib;

int parsePlyHeader(p_ply & input, std::map<std::string, e_ply_type> & attrs)
{
		if (!ply_read_header(input)) 
		{
			std::cerr<<"Partio: Problem parsing PLY header"<< inputFileName <<std::endl;
			return 0;
		}

		const char* lastComment = NULL;

		//display comments
		while ((lastComment = ply_get_next_comment(input, lastComment)))
		{
			printf("[PLY][Comment] %s\n",lastComment);
		}

		p_ply_element vertexElem = NULL;
		unsigned int vertexCount = 0;
		
		
		const char * elemName;
		long elemCount;
		while (vertexElem = ply_get_next_element(input, vertexElem))
		{
			ply_get_element_info(vertexElem, &elemName, &elemCount);
			printf("elem name %s count %i \n", elemName, elemCount);
			if (strcmp(elemName, "vertex") == 0){ //we only care about vertices
				vertexCount = elemCount;
				p_ply_property thisProp = NULL;
				while (thisProp = ply_get_next_property(vertexElem, thisProp))
				{
					const char * propName;
					e_ply_type  propType;

					ply_get_property_info(thisProp, &propName, &propType, NULL, NULL);
					printf("%s: type %i \n", propName, propType);

					plyVertexAttrib attr(std::string(propName), propType);
					attrs.insert( attr);
				}
			}
		}
		return vertexCount;
}

int setColourCallbacks(std::string r, std::string g, std::string b )
{	
	if ( (attrs.find(r) != attrs.end()) && (attrs.find(g) != attrs.end()) &&
			 (attrs.find(b) != attrs.end()))
		{
			colHandle = simple->addAttribute("pointColor",  VECTOR, 3);	
			Partio::ParticleAttributeType colorType = typePLY2Partio ( attrs[r] ); //we assume r g b are all the same colour
			switch (colorType)
			{
			case Partio::ParticleAttributeType::INT:
				ply_set_read_cb(input, "vertex", r.c_str(),   vertex_col_cb_uint, simplePtr, 0);
				ply_set_read_cb(input, "vertex", g.c_str(), vertex_col_cb_uint, simplePtr, 1);
				ply_set_read_cb(input, "vertex", b.c_str(),  vertex_col_cb_uint, simplePtr, 2);
			break;
			case Partio::ParticleAttributeType::FLOAT:
				ply_set_read_cb(input, "vertex", r.c_str(),   vertex_col_cb_float, simplePtr, 0);
				ply_set_read_cb(input, "vertex", g.c_str(), vertex_col_cb_float, simplePtr, 1);
				ply_set_read_cb(input, "vertex", b.c_str(),  vertex_col_cb_float, simplePtr, 2);
			}

			attrs.erase(r);
			attrs.erase(g);
			attrs.erase(b);
			return 1;
		}
	return 0;
}



	// TODO: convert this to use iterators like the rest of the readers/writers

	ParticlesDataMutable* readPLY(const char* filename,const bool headersOnly)
	{
		inputFileName= string(filename);

		input = ply_open(filename, NULL, 0, NULL);

		if (!input)
		{
			cerr<<"Partio: Can't open particle data file: "<<inputFileName<<endl;
			return 0;
		}

		if (headersOnly)
		{
			simple=new ParticleHeaders;
		}
		else simple=create();
		//printf("created!\n");  

		unsigned int nParticles = parsePlyHeader(input, attrs);


		// create null pointer to partio stuct for callbacks
		simplePtr = reinterpret_cast<void *>(simple);

		if ( (attrs.find("x") != attrs.end()) &&
			 (attrs.find("y") != attrs.end()) &&
			 (attrs.find("z") != attrs.end()) )
		{
			idHandle =	simple->addAttribute("id",  Partio::INT, 1);
			posHandle = simple->addAttribute("position",  VECTOR, 3);
			
			ply_set_read_cb(input, "vertex", "x", vertex_pos_cb, simplePtr, 0);
			ply_set_read_cb(input, "vertex", "y", vertex_pos_cb, simplePtr, 1);
			ply_set_read_cb(input, "vertex", "z", vertex_pos_cb, simplePtr, 2);
			
			attrs.erase("x");
			attrs.erase("y");
			attrs.erase("z");
		} else {
			cerr<<"Partio: Problem parsing PLY properties in file "<<inputFileName
				<< "\n No x,y,z coordinates found"<<endl;
		}

		int nCol = 0;
		nCol += setColourCallbacks("r", "g", "b");
		nCol += setColourCallbacks("red", "green", "blue");
		nCol += setColourCallbacks("R", "G", "B");
		if (nCol == 0) 	{
			 cout<<"Partio: No colors in file: "<<inputFileName<<endl;
		}

		int i = 0;
		for(std::map<std::string, e_ply_type>::iterator it = attrs.begin(); it != attrs.end(); it++) {
			genericHandles.push_back(simple->addAttribute(it->first.c_str(),  VECTOR, 1));

			ply_set_read_cb(input, "vertex", it->first.c_str(), generic_vertex_cb, simplePtr, i);
			cout << "set callback for leftover attribute: " << it->first <<" typt: " << it->second << endl;
			i++;
		}

		// all is set up now, proceed with parsing the file (if no headersOnly)
		if (!headersOnly)
			{
			simple->addParticles(nParticles);
			if (!ply_read(input)){
				cerr<<"Partio: Problem parsing PLY data in file: "<<inputFileName<<endl;
				return 0;
			}
		}
		ply_close(input);
		//printf("read!\n");
		return simple;
	}

}