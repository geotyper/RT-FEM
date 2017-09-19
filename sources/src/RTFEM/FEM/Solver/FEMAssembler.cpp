#include "RTFEM/FEM/Solver/FEMAssembler.h"

#include <RTFEM/Memory/UniquePointer.h>
#include <RTFEM/FEM/FEMModel.h>
#include <RTFEM/FEM/FEMGeometry.h>
#include <RTFEM/FEM/Solver/FiniteElementSolver.h>
#include <RTFEM/FEM/Solver/FiniteElementSolvers/TetrahedronSolver/TetrahedronSolver.h>
#include <RTFEM/FEM/FiniteElements/FiniteElementType.h>
#include <RTFEM/FEM/FiniteElement.h>
#include <RTFEM/FEM/Vertex.h>
#include <RTFEM/FEM/BoundaryCondition.h>

#include <iostream>

namespace rtfem {

template<class T>
FEMAssemblerData<T> FEMAssembler<T>::Compute(
    const std::shared_ptr<FEMModel<T>> fem_model) {
    auto global_dof_count =
        DIMENSION_COUNT * fem_model->fem_geometry().vertices.size();
    FEMAssemblerData<T> fem_assembler_data(global_dof_count);

    auto constitutive_matrix_C = ComputeConstitutiveMatrix(fem_model);

    ComputeAssemblerData(fem_assembler_data,
                         fem_model->fem_geometry(),
                         constitutive_matrix_C);

    ApplyBoundaryConditions(fem_assembler_data,
                            fem_model->boundary_conditions());

    return fem_assembler_data;
}

template<class T>
void FEMAssembler<T>::ComputeAssemblerData(
    FEMAssemblerData<T> &fem_assembler_data,
    const FEMGeometry<T> &fem_geometry,
    Eigen::Matrix<T, CONSTITUTIVE_MATRIX_N, CONSTITUTIVE_MATRIX_N> &
    constitutive_matrix_C) {
    for (const auto &finite_element : fem_geometry.finite_elements) {
        auto finite_element_solver =
            GetFiniteElementSolver(finite_element->type());

        auto finite_element_solver_data = finite_element_solver->Solve(
            finite_element, fem_geometry.vertices);

        auto boolean_assembly_matrix_A = ComputeBooleanAssemblyMatrix(
            finite_element, fem_geometry.vertices);

        fem_assembler_data.global_stiffness +=
            ComputePartialGlobalStiffnessMatrix(
                finite_element_solver_data.geometry_matrix,
                constitutive_matrix_C,
                boolean_assembly_matrix_A,
                finite_element_solver_data.volume);
        fem_assembler_data.global_force +=
            ComputePartialGlobalForceVector(
                finite_element_solver_data.force_vector,
                boolean_assembly_matrix_A);
    }
}

template<class T>
Eigen::Matrix<T, CONSTITUTIVE_MATRIX_N, CONSTITUTIVE_MATRIX_N>
FEMAssembler<T>::ComputeConstitutiveMatrix(const std::shared_ptr<FEMModel<T>> fem_model) {
    Eigen::Matrix<T, CONSTITUTIVE_MATRIX_N, CONSTITUTIVE_MATRIX_N>
        constitutive_matrix =
        Eigen::Matrix<T, CONSTITUTIVE_MATRIX_N, CONSTITUTIVE_MATRIX_N>::Zero();

    auto &material = fem_model->material();
    T p = material.poisson_coefficient;
    T e = material.young_modulus;
    T a = 1.0 - p;
    T b = (1.0 - 2.0 * p) / 2.0;
    T c = e / ((1.0 + p) * (1.0 - 2.0 * p));

    constitutive_matrix(0, 0) = a;
    constitutive_matrix(0, 1) = p;
    constitutive_matrix(0, 2) = p;
    constitutive_matrix(1, 0) = p;
    constitutive_matrix(1, 1) = a;
    constitutive_matrix(1, 2) = p;
    constitutive_matrix(2, 0) = p;
    constitutive_matrix(2, 1) = p;
    constitutive_matrix(2, 2) = a;
    constitutive_matrix(3, 3) = b;
    constitutive_matrix(4, 4) = b;
    constitutive_matrix(5, 5) = b;

    constitutive_matrix *= c;

    return constitutive_matrix;
}

template<class T>
std::unique_ptr<FiniteElementSolver<T>>
FEMAssembler<T>::GetFiniteElementSolver(const FiniteElementType &type) {
    switch (type) {
        case FiniteElementType::Tetrahedron:
            return rtfem::make_unique<TetrahedronSolver<T>>();
    }

    return nullptr;
}

template<class T>
Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>
FEMAssembler<T>::ComputeBooleanAssemblyMatrix(
    const std::shared_ptr<FiniteElement<T>> finite_element,
    const std::vector<std::shared_ptr<Vertex<T>>> &vertices) {
    unsigned int vertex_count = vertices.size();
    auto local_vertex_count = finite_element->GetVertexCount();

    unsigned int n = DIMENSION_COUNT * local_vertex_count;
    unsigned int m = DIMENSION_COUNT * vertex_count;
    Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> A
        = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>::Zero(n, m);

    for (unsigned int i = 0; i < local_vertex_count; i++) {
        const auto &vertex = vertices[finite_element->vertices_indices()[i]];
        auto global_id = vertex->id();

        auto global_column_start = global_id * DIMENSION_COUNT;
        auto local_row_start = i * DIMENSION_COUNT;

        A(local_row_start + 0, global_column_start + 0) = 1;
        A(local_row_start + 1, global_column_start + 1) = 1;
        A(local_row_start + 2, global_column_start + 2) = 1;
    }
    return A;
}

template<class T>
Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>
FEMAssembler<T>::ComputePartialGlobalStiffnessMatrix(
    const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> &geometry_matrix,
    const Eigen::Matrix<T,
                        CONSTITUTIVE_MATRIX_N,
                        CONSTITUTIVE_MATRIX_N> &constitutive_matrix_C,
    const Eigen::Matrix<T,
                        Eigen::Dynamic,
                        Eigen::Dynamic> &boolean_assembly_matrix_A,
    T volume) {
    auto local_stiffness_k = ComputeLocalStiffness(geometry_matrix,
                                                   constitutive_matrix_C,
                                                   volume);

    return boolean_assembly_matrix_A.transpose() * local_stiffness_k
        * boolean_assembly_matrix_A;
}

template<class T>
Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>
FEMAssembler<T>::ComputeLocalStiffness(
    const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> &geometry_matrix,
    const Eigen::Matrix<T,
                        CONSTITUTIVE_MATRIX_N,
                        CONSTITUTIVE_MATRIX_N> &constitutive_matrix,
    T volume) {
    auto &C = constitutive_matrix;
    auto &B = geometry_matrix;
    auto BT = geometry_matrix.transpose();

    auto local_stiffness = volume * (BT * C * B);

    return local_stiffness;
}

template<class T>
Eigen::Vector<T, Eigen::Dynamic>
FEMAssembler<T>::ComputePartialGlobalForceVector(
    const Eigen::Vector<T, Eigen::Dynamic> &force_vector,
    const Eigen::Matrix<T,
                        Eigen::Dynamic,
                        Eigen::Dynamic> &boolean_assembly_matrix_A) {
    return boolean_assembly_matrix_A.transpose() * force_vector;
}

template<class T>
void FEMAssembler<T>::ApplyBoundaryConditions(
    FEMAssemblerData<T> &assembler_data,
    const std::vector<BoundaryCondition<T>> &boundary_conditions) {

    for (auto &boundary_condition : boundary_conditions) {
        auto start_index = boundary_condition.vertex_id * DIMENSION_COUNT;

        for (unsigned int i = 0; i < DIMENSION_COUNT; i++) {
            auto boundary_value = boundary_condition.value[i];

            auto column = assembler_data.global_stiffness.col(start_index + i);
            for (unsigned int c = 0; c < column.size(); c++) {
                assembler_data.global_force[c] -= boundary_value * column[c];
            }
        }
    }

    for (auto &boundary_condition : boundary_conditions) {
        auto start_index = boundary_condition.vertex_id * DIMENSION_COUNT;

        for (unsigned int i = 0; i < DIMENSION_COUNT; i++) {
            auto boundary_value = boundary_condition.value[i];

            auto column = assembler_data.global_stiffness.col(start_index + i);
            for (unsigned int c = 0; c < column.size(); c++) {
                auto index = start_index + i;
                if (index == c) {
                    assembler_data.global_force[c] = boundary_value;
                    column[c] = 1;
                } else {
                    column[c] = 0;
                }
            }
        }

        for (unsigned int i = 0; i < DIMENSION_COUNT; i++) {
            auto index = start_index + i;
            auto row = assembler_data.global_stiffness.row(index);
            for (unsigned int r = 0; r < row.size(); r++) {
                if (r == index)
                    row[r] = 1;
                else
                    row[r] = 0;
            }
        }
    }
}

template
class FEMAssembler<double>;
template
class FEMAssembler<float>;

template
struct FEMAssemblerData<double>;
template
struct FEMAssemblerData<float>;

}