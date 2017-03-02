#include <wrap/system/qgetopt.h>

/*#include <vcg/complex/complex.h>
#include <vcg/complex/algorithms/update/normal.h> */


#include "mesh.h"
#include <wrap/io_trimesh/import.h>
#include <wrap/io_trimesh/export.h>

#include "mesh_coder.h"

//#include "../golomb.h"
#include "watch.h"
//#include "mesh.h"
#include "../common/nexus.h"


using namespace std;
using namespace vcg;

/* quantization: we would like to restore the original coordinate space
 * int offset x, y, z + scale (float power of 2) for each node.
 * each node quantized at precision, with border quantized precision +1
 */

int main(int argc, char *argv[]) {

    QString input; //nexus or ply
    QString output; //nexus or ply

    float precision; //base precision
    int norm_q(10);
    int red_q(5);
    int green_q(6);
    int blue_q(5);  //color quantization

    GetOpt opt(argc, argv);
    opt.addArgument("input", "filename of the nxs or ply", &input);
    opt.addArgument("output", "filename of the nxs or ply", &output);
    opt.addOption('p', "precision q", "quantization step (alternative to -v)", &step);
    opt.addOption('n', "normals q.", "number of normal quantization bits", &norm_q);
    opt.addOption('r', "red q.", "number of red channel quantization bits", &red_q);
    opt.addOption('g', "green q.", "number of green channel quantization bits", &green_q);
    opt.addOption('b', "blue q.", "number of blue channel quantization bits", &blue_q);
    opt.parse();

    //validate paraemters
    assert(norm_q > 4 && norm_q < 23);
    assert(red_q > 0 && red_q < 9);
    assert(green_q > 0 && green_q < 9);
    assert(blue_q > 0 && blue_q < 9);
/*
    if(input.endsWith(".ply")) {
        CMesh mesh;
        if(vcg::tri::io::ImporterPLY<CMesh>::Open(mesh,input.toLatin1().data())!=0) {
            cerr << "Error reading file " << qPrintable(input) << endl;
            return -1;
        }

        cout << "Mesh v: " << mesh.vn << " f: " << mesh.fn << endl;

        vcg::tri::UpdateNormal<CMesh>::PerVertexNormalized(mesh);
    } */

    nx::NexusData in;
    in.open(input.toLatin1().data());

    QFile file;
    file.setFileName(output);
    if(!file.open(QIODevice::ReadWrite))
        throw "Could not open file: " + output + " for writing";

    nx::Header header = nexus->header;
    header.signature = signature;
    header.nvert = 0;
    header.nface = 0;

    if(transform)
        header.sphere.Center() = matrix * header.sphere.Center();

    vector<nx::Node> nodes;
    vector<nx::Patch> patches;
    vector<nx::Texture> textures;

    uint n_nodes = nexus->header.n_nodes;
    vector<int> node_remap(n_nodes, -1);


    for(uint i = 0; i < n_nodes-1; i++) {
        if(!selected[i]) continue;
        nx::Node node = nexus->nodes[i];
        if(transform)
            node.sphere.Center() = matrix * node.sphere.Center();
        header.nvert += node.nvert;
        header.nface += node.nface;

        node_remap[i] = nodes.size();

        node.first_patch = patches.size();
        for(uint k = nexus->nodes[i].first_patch; k < nexus->nodes[i].last_patch(); k++) {
            nx::Patch patch = nexus->patches[k];
            patches.push_back(patch);
        }

        nodes.push_back(node);
    }

    nx::Node sink = nexus->nodes[n_nodes -1];
    sink.first_patch = patches.size();
    nodes.push_back(sink);

    for(uint i = 0; i < patches.size(); i++) {
        nx::Patch &patch = patches[i];
        int remapped = node_remap[patch.node];
        if(remapped == -1)
            patch.node = nodes.size()-1;
        else
            patch.node = remapped;
        assert(patch.node < nodes.size());
    }

    header.n_nodes = nodes.size();
    header.n_patches = patches.size();

    quint64 size = sizeof(nx::Header)  +
            nodes.size()*sizeof(nx::Node) +
            patches.size()*sizeof(nx::Patch) +
            textures.size()*sizeof(nx::Texture);
    size = pad(size);

    for(uint i = 0; i < nodes.size(); i++) {
        nodes[i].offset += size/NEXUS_PADDING;
    }

    file.write((char *)&header, sizeof(header));
    file.write((char *)&*nodes.begin(), sizeof(nx::Node)*nodes.size());
    file.write((char *)&*patches.begin(), sizeof(nx::Patch)*patches.size());
    file.seek(size);

    for(uint i = 0; i < node_remap.size()-1; i++) {
        int n = node_remap[i];
        if(n == -1) continue;
        nx::Node &node = nodes[n];
        node.offset = file.pos()/NEXUS_PADDING;

        nexus->loadRam(i);

        NodeData &data = nexus->nodedata[i];
        char *memory = data.memory;
        int data_size = nexus->nodes[i].getSize();
        if(transform) {
            memory = new char[node.getSize()];
            memcpy(memory, data.memory, data_size);

            for(int k = 0; k < node.nvert; k++) {
                vcg::Point3f &v = ((vcg::Point3f *)memory)[k];
                v = matrix * v;
            }
        }
        if(signature.flags & nx::Signature::CTM1) {
            compress(file, signature, node, nexus->nodedata[i]);
        } else {
            file.write(memory, data_size);
        }
        if(transform)
            delete []memory;

        nexus->dropRam(i);
    }
    nodes.back().offset = file.pos()/NEXUS_PADDING;
    file.seek(sizeof(nx::Header));
    file.write((char *)&*nodes.begin(), sizeof(nx::Node)*nodes.size());
    file.close();







































        //convert mesh to patch;
        Signature signature;
        signature.vertex.setComponent(VertexElement::COORD, nx::Attribute(nx::Attribute::FLOAT, 3));
        signature.face.setComponent(FaceElement::INDEX, nx::Attribute(nx::Attribute::UNSIGNED_SHORT, 3));
        if(!no_normals)
            signature.vertex.setComponent(nx::VertexElement::NORM, nx::Attribute(nx::Attribute::SHORT, 3));

        Patch patch;
        patch.allocate(signature, mesh.vert.size(), mesh.face.size()); //remember to delete patch
        for(int i = 0; i < mesh.vert.size(); i++) {
            patch.coords[i] = mesh.vert[i].P();
            if(signature.vertex.hasNormals()) {
                vcg::Point3f n = mesh.vert[i].N();
                patch.normals[i] = vcg::Point3s(n[0]*32767, n[1]*32767, n[2]*32767);
            }
        }
        for(int i = 0; i < mesh.face.size(); i++) {
            for(int k = 0; k < 3; k++) {
                patch.triangles[i*3 + k] = mesh.face[i].V(k) - &*mesh.vert.begin();
                assert(patch.triangles[i*3 + k] < patch.nvert);
            }
        }

        cout << "Patch: v: " << patch.nvert << " f: " << patch.nface << endl;

        float precision = step;

        OutputStream stream;
        MeshCoder encoder;

        if(precision > 0)
            encoder.geometry.setCoordQSide(precision);
        else
            encoder.geometry.setCoordQ(coord_q);
        encoder.geometry.setNormalQ(norm_q);
        encoder.geometry.setRGBQ(red_q, green_q, blue_q);


        Watch watch;
        watch.start();
        encoder.encode(stream, patch, signature);
        watch.pause();

        cout << "Coords bpv      : " << 8*encoder.coord_size/(float)mesh.vert.size() << endl;
        cout << "Normal bpv      : " << 8*encoder.normal_size/(float)mesh.vert.size() << endl;
        cout << "Connectivity bpv: " << 8*encoder.face_size/(float)mesh.vert.size() << endl;

        int s_size = stream.size();
        int size = mesh.vert.size()*(3*4) + mesh.face.size()*3*2; //v*3*4+2*3*2 ... 24 Bpv -> 192 bpv -> ~33 ma potrei
        if(signature.vertex.hasNormals())
            size += mesh.vert.size()*(3*2);
        cout << "Total compression: " << 100*s_size/(float)size << "%\n";
        cout << "Encoding triangles per sec: " << 2*mesh.vert.size()/watch.time() << endl << endl;

        CMesh decoded;
        Patch repatch;

        watch.start();
        int nface = 0;
        for(int i = 0; i < 100; i++) {
            InputStream istream(&*stream.begin(), stream.size());
            MeshCoder decoder;
            decoder.decode(istream, repatch, signature);
            nface += 2*repatch.nvert;
        }
        watch.pause();
        double t1 = watch.time();
        cout << "Decode tri per sec: " << nface/t1 << " face: " << nface << " time: " << t1 << endl;

        int savemask = vcg::tri::io::Mask::IOM_VERTCOORD | vcg::tri::io::Mask::IOM_VERTNORMAL | vcg::tri::io::Mask::IOM_FACEINDEX;

        //write mesh
        decoded.vert.resize(repatch.nvert);
        decoded.face.resize(repatch.nface);
        for(int i = 0; i < repatch.nvert; i++) {
            decoded.vert[i].P() = repatch.coords[i];
            //cout << "p: " << patch.coords[i][0] << endl;
        }

        for(int i = 0; i < repatch.nface; i++) {
            CFace &face = decoded.face[i];
            for(int k =0; k < 3; k++) {
                face.V(k) = &(decoded.vert[repatch.triangles[3*i+k]]);
            }
        }
        decoded.vn = repatch.nvert;
        decoded.fn = repatch.nface  ;
        vcg::tri::io::ExporterPLY<CMesh>::Save(decoded, "prova.ply", savemask, false);
        /*

  vcg::Box3f box;
  for(unsigned int i = 0; i < mesh.vert.size(); i++)
    box.Add(mesh.vert[i].P());


  BOTree<CVertex> botree;
  vector<int> order;
  ByteStream stream;
  botree.encode(stream, mesh.vert, order, box, box.Diag()/precision);
  cout << "Stream.size: " << stream.size() << endl;
  CVertex *begin = &*mesh.vert.begin();
  for(int i = 0; i < mesh.face.size(); i++) {
    CFace &f = mesh.face[i];
    for(int k = 0; k < 3; k++) {
      f.V(k) = begin + order[f.V(k) - begin];
    }
    if(f.V(0) == f.V(1) || f.V(0) == f.V(2) || f.V(1) == f.V(2))
      tri::Allocator<CMesh>::DeleteFace(mesh, f);
  }
  tri::Allocator<CMesh>::CompactFaceVector(mesh);
  //sort(mesh.face.begin(), mesh.face.end()); //dangerous and not needed!
  vcg::tri::UpdateTopology<CMesh>::FaceFace(mesh);

  int before = stream.size();

  StrMesh str_mesh(stream);
  str_mesh.encode(mesh, order);

  watch.pause();
  int connectivity = stream.size() - before;
  cout << "Seconds: " << watch.time() << endl;
  cout << "Tri per sec: " << mesh.face.size()/watch.time() << endl;
  cout << "Bytes per tri: " << connectivity/(float)mesh.face.size() << endl;

  int s_size = stream.size();
  cout << "Stream size: " << s_size << endl;
  int size = mesh.vert.size()*(3*4 + 3*2) + mesh.face.size()*3*2;
  cout << "Compared to: " << size << endl;
  cout << "reduction to: " << 100*s_size/(float)size << "%\n"; */
        /*  QApplication a(argc, argv);
  MainWindow w;
  w.show();
  return a.exec(); */

        /*
  int prova[] = { 2, 2, 1, 0, 0, 1, 2, 0, 2, 0, 0 };

  QsRangeEncoder32 encode(stream, 3);
  encode.encodeChar(11);
  for(int i = 0; i < 11; i++)
    encode.encodeSymbol(prova[i]);
  encode.flush();

  QsRangeDecoder32 decode(stream, 3);
  int c = decode.decodeChar();
  vector<int> d;


  for(int i = 0; i < c; i++)
    d.push_back(decode.decodeSymbol());

  cout << "Decoded: ";
  for(int i = 0; i < c; i++)
    cout << d[i];
  cout << endl;

  return -1; */
        return 0;
    }
