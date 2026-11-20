// Peekstep - STEP loading and meshing.
#include "step/StepLoader.h"

#include <BRepBndLib.hxx>
#include <BRepMesh_Context.hxx>
#include <BRepMesh_DelabellaMeshAlgoFactory.hxx>
#include <BRepMesh_FaceDiscret.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeFix_Wire.hxx>
#include <Precision.hxx>
#include <Bnd_Box.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Poly_Triangulation.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_ColorRGBA.hxx>
#include <OSD_Parallel.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <Standard_Version.hxx>
#if OCC_VERSION_HEX >= 0x070900
#include <ShapeProcess.hxx>
#endif
#include <TCollection_HAsciiString.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <NCollection_DataMap.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_Sequence.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_MaterialTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_VisMaterial.hxx>
#include <XCAFDoc_VisMaterialTool.hxx>
#include <XCAFPrs.hxx>
#include <XCAFPrs_DocumentExplorer.hxx>
#include <XCAFPrs_Style.hxx>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <stdexcept>

namespace peekstep {
namespace {

using Clock = std::chrono::steady_clock;
using FaceIndexMap = NCollection_DataMap<TopoDS_Shape, int, TopTools_ShapeMapHasher>;
using StyleMap = NCollection_IndexedDataMap<TopoDS_Shape, XCAFPrs_Style, TopTools_ShapeMapHasher>;
using LabelSeq = NCollection_Sequence<TDF_Label>;
using ShapeMap = NCollection_DataMap<TopoDS_Shape, TopoDS_Shape, TopTools_ShapeMapHasher>;

void meshShape(const TopoDS_Shape& shape, double deflection, bool inParallel)
{
    IMeshTools_Parameters params;
    params.Deflection = deflection;
    params.Angle = 0.35;
    params.InParallel = inParallel;
    // Delabella triangulates big planar faces (board outline, copper zones
    // with thousands of holes) far faster than the default algorithm.
    Handle(BRepMesh_Context) ctx = new BRepMesh_Context();
    ctx->SetFaceDiscret(new BRepMesh_FaceDiscret(new BRepMesh_DelabellaMeshAlgoFactory()));
    BRepMesh_IncrementalMesh mesher;
    mesher.SetShape(shape);
    mesher.ChangeParameters() = params;
    mesher.Perform(ctx);
}

// Adds the 2D (parametric) edge curves that some faces lack. The shape is
// changed in place, so faces of one shape must not be fixed from two threads.
void addMissingPCurves(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face face = TopoDS::Face(ex.Current().Oriented(TopAbs_FORWARD));
        bool missing = false;
        for (TopExp_Explorer e(face, TopAbs_EDGE); e.More() && !missing; e.Next()) {
            const TopoDS_Edge& edge = TopoDS::Edge(e.Current());
            Standard_Real first, last;
            missing = !BRep_Tool::Degenerated(edge) && BRep_Tool::CurveOnSurface(edge, face, first, last).IsNull();
        }
        if (!missing)
            continue;
        for (TopExp_Explorer w(face, TopAbs_WIRE); w.More(); w.Next()) {
            ShapeFix_Wire fix(TopoDS::Wire(w.Current()), face, Precision::Confusion());
            fix.FixEdgeCurves();
        }
    }
}

double secondsSince(Clock::time_point t0)
{
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

std::string toStd(const Handle(TCollection_HAsciiString)& s)
{
    return s.IsNull() ? std::string() : std::string(s->ToCString());
}

// Grey used when the file says nothing about a part's color.
const Quantity_ColorRGBA kDefaultColor(Quantity_Color(0.70, 0.70, 0.72, Quantity_TOC_sRGB), 1.0f);

bool styleColor(const XCAFPrs_Style& style, Quantity_ColorRGBA& out)
{
    if (style.IsSetColorSurf()) {
        out = style.GetColorSurfRGBA();
        return true;
    }
    if (!style.Material().IsNull()) {
        out = style.Material()->BaseColor();
        return true;
    }
    return false;
}

// Colors are stored as sRGB bytes; the shaders work in sRGB too, which is
// what CAD users expect when they compare colors with other tools.
std::array<float, 4> toSrgb(const Quantity_ColorRGBA& c)
{
    double r, g, b;
    c.GetRGB().Values(r, g, b, Quantity_TOC_sRGB);
    return {float(r), float(g), float(b), c.Alpha()};
}

uint32_t packColor(const std::array<float, 4>& c)
{
    auto b = [](float v) { return uint32_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return b(c[0]) | (b(c[1]) << 8) | (b(c[2]) << 16) | (b(c[3]) << 24);
}

struct Leaf {
    TopoDS_Shape shape;       // part shape without the instance location
    TopLoc_Location location; // where this instance sits in the assembly
    TDF_Label label;          // part (referred) label, for per-face styles
    Quantity_ColorRGBA color; // instance color, inherited from the assembly
};

// Maps each face of a part to a color, honouring colors set on sub-shapes
// (solids, shells, faces). More specific shapes win.
void collectFaceColors(const Leaf& leaf, FaceIndexMap& faceColor,
                       std::vector<Quantity_ColorRGBA>& palette)
{
    StyleMap styles;
    XCAFPrs::CollectStyleSettings(leaf.label, TopLoc_Location(), styles);

    std::vector<std::pair<TopoDS_Shape, Quantity_ColorRGBA>> entries;
    for (StyleMap::Iterator it(styles); it.More(); it.Next()) {
        Quantity_ColorRGBA c;
        if (styleColor(it.Value(), c))
            entries.emplace_back(it.Key(), c);
    }
    // TopAbs_ShapeEnum goes from COMPOUND (0) to FACE (4): apply coarse first.
    std::stable_sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.first.ShapeType() < b.first.ShapeType();
    });
    for (const auto& [shape, color] : entries) {
        palette.push_back(color);
        const int idx = int(palette.size()) - 1;
        for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next())
            faceColor.Bind(ex.Current(), idx);
    }
}

void appendFace(const TopoDS_Face& original, const TopLoc_Location& instanceLoc, uint32_t color,
                const ShapeMap& healedFaces, Model& model)
{
    // Use the healed copy of a face that did not mesh, placed like the original.
    TopoDS_Face face = original;
    if (const TopoDS_Shape* fixed = healedFaces.Seek(original.Located(TopLoc_Location()).Oriented(TopAbs_FORWARD)))
        face = TopoDS::Face(fixed->Located(original.Location()).Oriented(original.Orientation()));

    TopLoc_Location faceLoc;
    const Handle(Poly_Triangulation)& tri = BRep_Tool::Triangulation(face, faceLoc);
    if (tri.IsNull() || tri->NbTriangles() == 0) {
        ++model.unmeshedFaces;
        return;
    }

    const gp_Trsf trsf = (instanceLoc * faceLoc).Transformation();
    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    const uint32_t base = uint32_t(model.vertices.size());
    const int nbNodes = tri->NbNodes();

    std::vector<gp_Vec> normals(size_t(nbNodes), gp_Vec(0, 0, 0));
    for (int i = 1; i <= nbNodes; ++i) {
        gp_Pnt p = tri->Node(i).Transformed(trsf);
        Vertex v{};
        v.pos[0] = float(p.X());
        v.pos[1] = float(p.Y());
        v.pos[2] = float(p.Z());
        v.color = color;
        model.vertices.push_back(v);
    }

    for (int t = 1; t <= tri->NbTriangles(); ++t) {
        int n1, n2, n3;
        tri->Triangle(t).Get(n1, n2, n3);
        if (reversed)
            std::swap(n2, n3);
        const Vertex& a = model.vertices[base + n1 - 1];
        const Vertex& b = model.vertices[base + n2 - 1];
        const Vertex& c = model.vertices[base + n3 - 1];
        gp_Vec ab(b.pos[0] - a.pos[0], b.pos[1] - a.pos[1], b.pos[2] - a.pos[2]);
        gp_Vec ac(c.pos[0] - a.pos[0], c.pos[1] - a.pos[1], c.pos[2] - a.pos[2]);
        gp_Vec n = ab.Crossed(ac); // area weighted
        normals[n1 - 1] += n;
        normals[n2 - 1] += n;
        normals[n3 - 1] += n;
        model.indices.push_back(base + n1 - 1);
        model.indices.push_back(base + n2 - 1);
        model.indices.push_back(base + n3 - 1);
    }

    for (int i = 0; i < nbNodes; ++i) {
        gp_Vec n = normals[size_t(i)];
        const double len = n.Magnitude();
        if (len > 0)
            n /= len;
        Vertex& v = model.vertices[base + i];
        v.normal[0] = float(n.X());
        v.normal[1] = float(n.Y());
        v.normal[2] = float(n.Z());
    }
}

void collectMaterials(const Handle(TDocStd_Document)& doc, Model& model)
{
    Handle(XCAFDoc_MaterialTool) matTool = XCAFDoc_DocumentTool::MaterialTool(doc->Main());
    LabelSeq labels;
    matTool->GetMaterialLabels(labels);
    for (const TDF_Label& l : labels) {
        Handle(TCollection_HAsciiString) name, desc, densName, densType;
        double density = 0;
        if (!matTool->GetMaterial(l, name, desc, density, densName, densType))
            continue;
        MaterialInfo m;
        m.name = toStd(name);
        m.description = toStd(desc);
        m.density = density;
        m.densityUnit = toStd(densName);
        model.materials.push_back(std::move(m));
    }

    Handle(XCAFDoc_VisMaterialTool) visTool = XCAFDoc_DocumentTool::VisMaterialTool(doc->Main());
    LabelSeq visLabels;
    visTool->GetMaterials(visLabels);
    for (const TDF_Label& l : visLabels) {
        Handle(XCAFDoc_VisMaterial) vm = visTool->GetMaterial(l);
        if (vm.IsNull())
            continue;
        MaterialInfo m;
        m.name = toStd(vm->RawName());
        if (m.name.empty())
            m.name = "(unnamed)";
        m.visual = true;
        model.materials.push_back(std::move(m));
    }
}

} // namespace

Model loadStep(const std::string& path, LoadProgress* progress)
{
    Model model;
    const auto t0 = Clock::now();

    Handle(TDocStd_Document) doc;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", doc);

    STEPCAFControl_Reader reader;
    reader.SetColorMode(true);
    reader.SetNameMode(true);
    reader.SetMatMode(true);
    reader.SetLayerMode(false);
    reader.SetPropsMode(false);
    reader.SetGDTMode(false);
    reader.SetSHUOMode(false);


    if (reader.ReadFile(path.c_str()) != IFSelect_RetDone)
        throw std::runtime_error("Could not read STEP file");
    model.parseSeconds = secondsSince(t0);

#if OCC_VERSION_HEX >= 0x070900
    // Skip shape healing. It is most of the conversion time, and files from
    // CAD tools (KiCad uses OpenCASCADE itself) rarely need it for viewing.
    // Set PEEKSTEP_HEAL=1 to turn it back on for a broken file.
    const char* heal = std::getenv("PEEKSTEP_HEAL");
    if (!(heal && heal[0] == '1'))
        reader.SetShapeProcessFlags(ShapeProcess::OperationsFlags());
#endif
    const auto tTransfer = Clock::now();
    if (!reader.Transfer(doc))
        throw std::runtime_error("Could not convert the STEP data");
    model.transferSeconds = secondsSince(tTransfer);

    if (progress)
        progress->stage = 1;
    const auto t1 = Clock::now();

    // Flatten the assembly into part instances.
    std::vector<Leaf> leaves;
    for (XCAFPrs_DocumentExplorer ex(doc, XCAFPrs_DocumentExplorerFlags_OnlyLeafNodes); ex.More(); ex.Next()) {
        const XCAFPrs_DocumentNode& node = ex.Current();
        Leaf leaf;
        leaf.label = node.RefLabel;
        leaf.shape = XCAFDoc_ShapeTool::GetShape(node.RefLabel);
        leaf.location = node.Location;
        if (leaf.shape.IsNull())
            continue;
        if (!styleColor(node.Style, leaf.color))
            leaf.color = kDefaultColor;
        leaves.push_back(std::move(leaf));
    }
    model.parts = leaves.size();
    if (leaves.empty())
        throw std::runtime_error("The file contains no shapes");

    ShapeMap healedFaces; // unlocated face that failed to mesh -> healed, meshed copy

    // Mesh each distinct part once (keyed without location, so the same
    // geometry is never meshed by two threads at once).
    struct MeshJob {
        TopoDS_Shape shape;
        int faces = 0;
        double deflection = 0;
    };
    std::vector<MeshJob> jobs;
    {
        FaceIndexMap seen;
        for (const Leaf& l : leaves) {
            const TopoDS_Shape key = l.shape.Located(TopLoc_Location());
            if (seen.IsBound(key))
                continue;
            seen.Bind(key, 0);
            MeshJob job;
            job.shape = key;
            for (TopExp_Explorer ex(key, TopAbs_FACE); ex.More(); ex.Next())
                ++job.faces;
            jobs.push_back(std::move(job));
        }
    }

    // The deflection follows the part size so that tiny components in a big
    // assembly still look smooth.
    for (MeshJob& job : jobs) {
        Bnd_Box box;
        BRepBndLib::Add(job.shape, box, false);
        job.deflection = box.IsVoid() ? 0.0 : std::max(std::sqrt(box.SquareExtent()) * 0.002, 1e-4);
    }
    auto meshPart = [](const MeshJob& job, bool inParallel) {
        if (job.deflection > 0)
            meshShape(job.shape, job.deflection, inParallel);
    };

    // Shape healing was skipped while reading, and about a quarter of the
    // faces in a KiCad export lack the 2D edge curves the mesher needs. Add
    // just those curves up front; it is much cheaper than healing the faces.
    OSD_Parallel::For(0, int(jobs.size()), [&](int i) { addMissingPCurves(jobs[size_t(i)].shape); });

    // Big parts (like the board itself) mesh their faces in parallel; the
    // many small parts are meshed in parallel with each other.
    constexpr int kBigPart = 300;
    std::vector<const MeshJob*> small;
    for (const MeshJob& job : jobs) {
        if (job.faces >= kBigPart)
            meshPart(job, true);
        else
            small.push_back(&job);
    }
    OSD_Parallel::For(0, int(small.size()), [&](int i) { meshPart(*small[size_t(i)], false); });

    // A few faces may still fail to mesh: heal just those faces and mesh
    // them again. Parts are handled in parallel, faces of one part in
    // sequence because they share edges.
    std::vector<std::vector<TopoDS_Face>> unmeshed(jobs.size());
    for (size_t i = 0; i < jobs.size(); ++i) {
        FaceIndexMap seen;
        for (TopExp_Explorer ex(jobs[i].shape, TopAbs_FACE); ex.More(); ex.Next()) {
            const TopoDS_Face& face = TopoDS::Face(ex.Current());
            TopLoc_Location loc;
            const Handle(Poly_Triangulation)& tri = BRep_Tool::Triangulation(face, loc);
            if (!tri.IsNull() && tri->NbTriangles() > 0)
                continue;
            const TopoDS_Shape key = face.Located(TopLoc_Location()).Oriented(TopAbs_FORWARD);
            if (!seen.IsBound(key)) {
                seen.Bind(key, 0);
                unmeshed[i].push_back(TopoDS::Face(key));
            }
        }
    }
    std::vector<std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>>> fixedFaces(jobs.size());
    OSD_Parallel::For(0, int(jobs.size()), [&](int i) {
        for (const TopoDS_Face& face : unmeshed[size_t(i)]) {
            Handle(ShapeFix_Face) fix = new ShapeFix_Face(face);
            fix->Perform();
            const TopoDS_Face fixed = fix->Face();
            meshShape(fixed, jobs[size_t(i)].deflection, false);
            fixedFaces[size_t(i)].emplace_back(face, fixed);
        }
    });
    for (const auto& list : fixedFaces) {
        for (const auto& [face, fixed] : list) {
            healedFaces.Bind(face, fixed);
            ++model.healedFaces;
        }
    }
    model.meshSeconds = secondsSince(t1);

    if (progress)
        progress->stage = 2;

    // Build the vertex and index buffers.
    const auto t2 = Clock::now();
    std::map<uint32_t, size_t> colorFaces;
    for (const Leaf& leaf : leaves) {
        FaceIndexMap faceColor;
        std::vector<Quantity_ColorRGBA> palette;
        collectFaceColors(leaf, faceColor, palette);
        const uint32_t instanceColor = packColor(toSrgb(leaf.color));

        for (TopExp_Explorer ex(leaf.shape, TopAbs_FACE); ex.More(); ex.Next()) {
            const TopoDS_Face& face = TopoDS::Face(ex.Current());
            uint32_t color = instanceColor;
            if (const int* idx = faceColor.Seek(face))
                color = packColor(toSrgb(palette[size_t(*idx)]));
            appendFace(face, leaf.location, color, healedFaces, model);
            ++colorFaces[color];
            ++model.faces;
        }
    }

    for (const auto& [packed, count] : colorFaces) {
        ColorInfo ci;
        for (int i = 0; i < 4; ++i)
            ci.rgba[size_t(i)] = float((packed >> (8 * i)) & 0xff) / 255.0f;
        ci.faces = count;
        model.colors.push_back(ci);
    }
    std::sort(model.colors.begin(), model.colors.end(),
              [](const ColorInfo& a, const ColorInfo& b) { return a.faces > b.faces; });

    collectMaterials(doc, model);

    if (!model.vertices.empty()) {
        model.bboxMin = {model.vertices[0].pos[0], model.vertices[0].pos[1], model.vertices[0].pos[2]};
        model.bboxMax = model.bboxMin;
        for (const Vertex& v : model.vertices) {
            for (int i = 0; i < 3; ++i) {
                model.bboxMin[size_t(i)] = std::min(model.bboxMin[size_t(i)], v.pos[i]);
                model.bboxMax[size_t(i)] = std::max(model.bboxMax[size_t(i)], v.pos[i]);
            }
        }
    }

    XCAFApp_Application::GetApplication()->Close(doc);
    model.buildSeconds = secondsSince(t2);
    return model;
}

} // namespace peekstep
