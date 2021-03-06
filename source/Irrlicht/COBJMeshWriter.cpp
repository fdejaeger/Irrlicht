// Copyright (C) 2008-2012 Christian Stehno
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_OBJ_WRITER_

#include "COBJMeshWriter.h"
#include "os.h"
#include "IMesh.h"
#include "IMeshBuffer.h"
#include "IAttributes.h"
#include "ISceneManager.h"
#include "IMeshCache.h"
#include "IWriteFile.h"
#include "IFileSystem.h"
#include "ITexture.h"

namespace irr
{
namespace scene
{

COBJMeshWriter::COBJMeshWriter(scene::ISceneManager* smgr, io::IFileSystem* fs)
	: SceneManager(smgr), FileSystem(fs)
{
	#ifdef _DEBUG
	setDebugName("COBJMeshWriter");
	#endif

	if (SceneManager)
		SceneManager->grab();

	if (FileSystem)
		FileSystem->grab();
}


COBJMeshWriter::~COBJMeshWriter()
{
	if (SceneManager)
		SceneManager->drop();

	if (FileSystem)
		FileSystem->drop();
}


//! Returns the type of the mesh writer
EMESH_WRITER_TYPE COBJMeshWriter::getType() const
{
	return EMWT_OBJ;
}


//! writes a mesh
bool COBJMeshWriter::writeMesh(io::IWriteFile* file, scene::IMesh* mesh, s32 flags)
{
	if (!file || !mesh)
		return false;

	for(u32 i = 0; i < mesh->getMeshBufferCount(); ++i)
	{
		u32 Size = mesh->getMeshBuffer(i)->getVertexBuffer()->getVertexSize();

		if(Size != sizeof(video::S3DVertex) && Size != sizeof(video::S3DVertex2TCoords) && Size != sizeof(video::S3DVertexTangents))
			return false;
	}

	os::Printer::log("Writing mesh", file->getFileName());

	// write OBJ MESH header

	io::path name;
	core::cutFilenameExtension(name,file->getFileName()) += ".mtl";
	file->write("# exported by Irrlicht\n",23);
	file->write("mtllib ",7);
	file->write(name.c_str(),name.size());
	file->write("\n\n",2);

	// write mesh buffers

	core::array<video::SMaterial*> mat;

	u32 allVertexCount=1; // count vertices over the whole file
	for (u32 i=0; i<mesh->getMeshBufferCount(); ++i)
	{
		core::stringc num(i+1);
		IMeshBuffer* buffer = mesh->getMeshBuffer(i);
		if (buffer && buffer->getVertexBuffer()->getVertexCount())
		{
			file->write("g grp", 5);
			file->write(num.c_str(), num.size());
			file->write("\n",1);

			u32 j;
			const u32 vertexCount = buffer->getVertexBuffer()->getVertexCount();

			u8* Vertices = static_cast<u8*>(buffer->getVertexBuffer()->getVertices());

			video::IVertexAttribute* attribute = buffer->getVertexDescriptor()->getAttributeBySemantic(video::EVAS_POSITION);

			if(attribute)
				for (j=0; j<vertexCount; ++j)
				{
					core::vector3df* value = (core::vector3df*)(Vertices + attribute->getOffset() + buffer->getVertexBuffer()->getVertexSize() * j);

					file->write("v ",2);
					getVectorAsStringLine(*value, num);
					file->write(num.c_str(), num.size());
				}

			attribute = buffer->getVertexDescriptor()->getAttributeBySemantic(video::EVAS_TEXCOORD1);

			if(attribute)
				for (j=0; j<vertexCount; ++j)
				{
					core::vector2df* value = (core::vector2df*)(Vertices + attribute->getOffset() + buffer->getVertexBuffer()->getVertexSize() * j);

					file->write("vt ",3);
					getVectorAsStringLine(*value, num);
					file->write(num.c_str(), num.size());
				}

			attribute = buffer->getVertexDescriptor()->getAttributeBySemantic(video::EVAS_TANGENT);

			if(attribute)
				for (j=0; j<vertexCount; ++j)
				{
					core::vector3df* value = (core::vector3df*)(Vertices + attribute->getOffset() + buffer->getVertexBuffer()->getVertexSize() * j);

					file->write("vn ",3);
					getVectorAsStringLine(*value, num);
					file->write(num.c_str(), num.size());
				}

			file->write("usemtl mat",10);
			num = "";
			for (j=0; j<mat.size(); ++j)
			{
				if (*mat[j]==buffer->getMaterial())
				{
					num = core::stringc(j);
					break;
				}
			}
			if (num == "")
			{
				num = core::stringc(mat.size());
				mat.push_back(&buffer->getMaterial());
			}
			file->write(num.c_str(), num.size());
			file->write("\n",1);

			const u32 indexCount = buffer->getIndexBuffer()->getIndexCount();
			for (j=0; j<indexCount; j+=3)
			{
				file->write("f ",2);
				num = core::stringc(buffer->getIndexBuffer()->getIndex(j+2)+allVertexCount);
				file->write(num.c_str(), num.size());
				file->write("/",1);
				file->write(num.c_str(), num.size());
				file->write("/",1);
				file->write(num.c_str(), num.size());
				file->write(" ",1);

				num = core::stringc(buffer->getIndexBuffer()->getIndex(j+1)+allVertexCount);
				file->write(num.c_str(), num.size());
				file->write("/",1);
				file->write(num.c_str(), num.size());
				file->write("/",1);
				file->write(num.c_str(), num.size());
				file->write(" ",1);

				num = core::stringc(buffer->getIndexBuffer()->getIndex(j+0)+allVertexCount);
				file->write(num.c_str(), num.size());
				file->write("/",1);
				file->write(num.c_str(), num.size());
				file->write("/",1);
				file->write(num.c_str(), num.size());
				file->write(" ",1);

				file->write("\n",1);
			}
			file->write("\n",1);
			allVertexCount += vertexCount;
		}
	}

	if (mat.size() == 0)
		return true;

	file = FileSystem->createAndWriteFile( name );
	if (file)
	{
		os::Printer::log("Writing material", file->getFileName());

		file->write("# exported by Irrlicht\n\n",24);
		for (u32 i=0; i<mat.size(); ++i)
		{
			core::stringc num(i);
			file->write("newmtl mat",10);
			file->write(num.c_str(),num.size());
			file->write("\n",1);

			getColorAsStringLine(mat[i]->AmbientColor, "Ka", num);
			file->write(num.c_str(),num.size());
			getColorAsStringLine(mat[i]->DiffuseColor, "Kd", num);
			file->write(num.c_str(),num.size());
			getColorAsStringLine(mat[i]->SpecularColor, "Ks", num);
			file->write(num.c_str(),num.size());
			getColorAsStringLine(mat[i]->EmissiveColor, "Ke", num);
			file->write(num.c_str(),num.size());
			num = core::stringc((double)(mat[i]->Shininess/0.128f));
			file->write("Ns ", 3);
			file->write(num.c_str(),num.size());
			file->write("\n", 1);
			if (mat[i]->getTexture(0))
			{
				file->write("map_Kd ", 7);
				io::path tname = FileSystem->getRelativeFilename(mat[i]->getTexture(0)->getName(),
						FileSystem->getFileDir(file->getFileName()));
				// avoid blanks as .obj cannot handle strings with spaces
				if (tname.findFirst(' ') != -1)
					tname = FileSystem->getFileBasename(tname);
				file->write(tname.c_str(), tname.size());
				file->write("\n",1);
			}
			file->write("\n",1);
		}
		file->drop();
	}
	return true;
}


void COBJMeshWriter::getVectorAsStringLine(const core::vector3df& v, core::stringc& s) const
{
	s = core::stringc(-v.X);
	s += " ";
	s += core::stringc(v.Y);
	s += " ";
	s += core::stringc(v.Z);
	s += "\n";
}


void COBJMeshWriter::getVectorAsStringLine(const core::vector2df& v, core::stringc& s) const
{
	s = core::stringc(v.X);
	s += " ";
	s += core::stringc(1-v.Y);
	s += "\n";
}


void COBJMeshWriter::getColorAsStringLine(const video::SColor& color, const c8* const prefix, core::stringc& s) const
{
	s = prefix;
	s += " ";
	s += core::stringc((double)(color.getRed()/255.f));
	s += " ";
	s += core::stringc((double)(color.getGreen()/255.f));
	s += " ";
	s += core::stringc((double)(color.getBlue()/255.f));
	s += "\n";
}


} // end namespace
} // end namespace

#endif

