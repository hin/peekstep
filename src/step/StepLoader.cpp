// Peekstep - STEP loading and meshing.
#include "step/StepLoader.h"

#include <BRepBndLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Poly_Triangulation.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_ColorRGBA.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TCollection_HAsciiString.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_DataMapOfShapeInteger.hxx>
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
#include <XCAFPrs_IndexedDataMapOfShapeStyle.hxx>
#include <XCAFPrs_Style.hxx>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <stdexcept>

namespace peekstep {
namespace {

using Clock = std::chrono::steady_clock;

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
    Standard_Real r, g, b;
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
void collectFaceColors(const Leaf& leaf, TopTools_DataMapOfShapeInteger& faceColor,
                       std::vector<Quantity_ColorRGBA>& palette)
{
    XCAFPrs_IndexedDataMapOfShapeStyle styles;
    XCAFPrs::CollectStyleSettings(leaf.label, TopLoc_Location(), styles);

    std::vector<std::pair<TopoDS_Shape, Quantity_ColorRGBA>> entries;
    for (XCAFPrs_IndexedDataMapOfShapeStyle::Iterator it(styles); it.More(); it.Next()) {
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

void appendFace(const TopoDS_Face& face, const TopLoc_Location& instanceLoc, uint32_t color, Model& model)
{
    TopLoc_Location faceLoc;
    const Handle(Poly_Triangulation)& tri = BRep_Tool::Triangulation(face, faceLoc);
    if (tri.IsNull() || tri->NbTriangles() == 0)
        return;

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
    TDF_LabelSequence labels;
    matTool->GetMaterialLabels(labels);
    for (const TDF_Label& l : labels) {
        Handle(TCollection_HAsciiString) name, desc, densName, densType;
        Standard_Real density = 0;
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
    TDF_LabelSequence visLabels;
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
    if (!reader.Transfer(doc))
        throw std::runtime_error("Could not convert the STEP data");
    model.readSeconds = secondsSince(t0);

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

    // Mesh each distinct part once. The deflection follows the part size so
    // that tiny components in a big assembly still look smooth.
    std::vector<TopoDS_Shape> unique;
    {
        TopTools_DataMapOfShapeInteger seen;
        for (const Leaf& l : leaves) {
            if (!seen.IsBound(l.shape)) {
                seen.Bind(l.shape, 0);
                unique.push_back(l.shape);
            }
        }
    }
    for (const TopoDS_Shape& shape : unique) {
        Bnd_Box box;
        BRepBndLib::Add(shape, box, false);
        if (box.IsVoid())
            continue;
        const double diag = std::sqrt(box.SquareExtent());
        IMeshTools_Parameters params;
        params.Deflection = std::max(diag * 0.002, 1e-4);
        params.Angle = 0.35;
        params.InParallel = true;
        BRepMesh_IncrementalMesh mesher(shape, params);
    }
    model.meshSeconds = secondsSince(t1);

    if (progress)
        progress->stage = 2;

    // Build the vertex and index buffers.
    std::map<uint32_t, size_t> colorFaces;
    for (const Leaf& leaf : leaves) {
        TopTools_DataMapOfShapeInteger faceColor;
        std::vector<Quantity_ColorRGBA> palette;
        collectFaceColors(leaf, faceColor, palette);
        const uint32_t instanceColor = packColor(toSrgb(leaf.color));

        for (TopExp_Explorer ex(leaf.shape, TopAbs_FACE); ex.More(); ex.Next()) {
            const TopoDS_Face& face = TopoDS::Face(ex.Current());
            uint32_t color = instanceColor;
            if (const int* idx = faceColor.Seek(face))
                color = packColor(toSrgb(palette[size_t(*idx)]));
            appendFace(face, leaf.location, color, model);
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
    return model;
}

} // namespace peekstep
