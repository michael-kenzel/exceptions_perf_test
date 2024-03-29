#include <utility>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <fstream>

#include "obj_stream.h"
#include "obj_reader.h"
#include "obj.h"

using namespace std::literals;


namespace
{
	std::size_t combineHashes(std::size_t a, std::size_t b)
	{
		// based on https://stackoverflow.com/a/27952689/2064761
		return a ^ (b + 0x9E3779B9U + (a << 6) + (a >> 2));
	}

	struct face_vertex_t
	{
		int v, n, t;

		friend constexpr bool operator ==(const face_vertex_t& a, const face_vertex_t& b)
		{
			return a.v == b.v && a.n == b.n && a.t == b.t;
		}
	};

	struct face_vertex_hash : private std::hash<int>
	{
		using std::hash<int>::operator();

		std::size_t operator ()(const face_vertex_t& v) const
		{
			return combineHashes(combineHashes((*this)(v.v), (*this)(v.n)), (*this)(v.t));
		}
	};


	class OBJConsumer
	{
		std::vector<float3> v;
		std::vector<float3> vn;
		std::vector<float2> vt;

		std::unordered_map<face_vertex_t, int, face_vertex_hash> vertex_map;

		std::vector<float3> positions;
		std::vector<float3> normals;
		std::vector<float2> texcoords;
		std::vector<std::array<int, 3>> triangles;

		static constexpr int MAX_FACE_VERTICES = 7;

		int face_vertices[MAX_FACE_VERTICES];
		int num_face_vertices = 0;

	public:
		OBJConsumer()
			: vn {{ 0.0f, 0.0f, 0.0f }}, vt {{ 0.0f, 0.0f }}
		{
		}

		void consumeVertex(OBJ::Stream& stream, float x, float y, float z)
		{
			v.emplace_back(x, y, z);
		}

		void consumeVertex(OBJ::Stream& stream, float x, float y, float z, float w)
		{
			stream.throwError("weighted vertex coordinates are not supported"sv);
		}

		void consumeNormal(OBJ::Stream& stream, float x, float y, float z)
		{
			vn.emplace_back(x, y, z);
		}

		void consumeTexcoord(OBJ::Stream& stream, float u)
		{
			stream.throwError("1D texture coordinates are not supported"sv);
		}

		void consumeTexcoord(OBJ::Stream& stream, float u, float v)
		{
			vt.emplace_back(u, 1.0f - v);
		}

		void consumeTexcoord(OBJ::Stream& stream, float u, float v, float w)
		{
			stream.throwError("3D texture coordinates are not supported"sv);
		}

		void consumeFaceVertex(OBJ::Stream& stream, int vi, int ni, int ti)
		{
			if (vi < 0)
				vi = static_cast<int>(size(v)) + vi;
			else
				--vi;

			if (ni < 0)
				ni = static_cast<int>(size(vn)) + ni;

			if (ti < 0)
				ti = static_cast<int>(size(vt)) + ti;

			auto [fv, inserted] = vertex_map.try_emplace({ vi, ni, ti }, static_cast<int>(size(positions)));

			if (inserted)
			{
				positions.push_back(v[vi]);
				normals.push_back(vn[ni]);
				texcoords.push_back(vt[ti]);
			}

			if (num_face_vertices >= MAX_FACE_VERTICES)
				stream.throwError("this face has too many vertices"sv);
			face_vertices[num_face_vertices++] = fv->second;
		}

		void finishFace(OBJ::Stream& stream)
		{
			if (num_face_vertices < 3)
				stream.throwError("face must have at least three vertices"sv);

			for (int i = 2; i < num_face_vertices; ++i)
				triangles.push_back({ face_vertices[0], face_vertices[i - 1], face_vertices[i] });

			num_face_vertices = 0;
		}

		void consumeObjectName(OBJ::Stream& stream, std::string_view name)
		{
		}

		void consumeGroupName(OBJ::Stream& stream, std::string_view name)
		{
		}

		void finishGroupAssignment(OBJ::Stream& streame)
		{
		}

		void consumeSmoothingGroup(OBJ::Stream& stream, int n)
		{
			stream.warn("smoothing groups are ignored!"sv);
		}

		void consumeMtlLib(OBJ::Stream& stream, std::string_view name)
		{
			stream.warn("materials are ignored!"sv);
		}

		void consumeUseMtl(OBJ::Stream& stream, std::string_view name)
		{
			stream.warn("materials are ignored!"sv);
		}

		OBJ::Triangles finish()
		{
			return { std::move(positions), std::move(normals), std::move(texcoords), std::move(triangles) };
		}
	};


	struct Buffer
	{
		std::unique_ptr<char[]> data;
		std::size_t size;
	};

	Buffer readFile(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary);

		if (!file)
			throw std::runtime_error("failed to open obj file");

		file.seekg(0, std::ios::end);
		auto size = static_cast<std::size_t>(file.tellg());
		file.seekg(0);
		auto data = std::unique_ptr<char[]> { new char[size] };
		file.read(&data[0], size);

		if (!file)
			throw std::runtime_error("failed to read obj file");

		return { std::move(data), size };
	}
}

namespace OBJ
{
	Triangles readTriangles(const char* begin, const char* end, std::string_view name, StreamCallback& stream_callback)
	{
		Stream stream(begin, end, name, stream_callback);
		OBJConsumer consumer;
		Reader<OBJConsumer> reader(consumer);
		stream.consume(reader);
		return consumer.finish();
	}

	Triangles readTriangles(const std::filesystem::path& path, StreamCallback& stream_callback)
	{
		auto [data, size] = readFile(path);
		return readTriangles(&data[0], &data[0] + size, path.filename().u8string(), stream_callback);
	}
}
