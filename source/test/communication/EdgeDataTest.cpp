/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and LICENSE.
 *
 * Copyright:     (c) 1997-2023 Lawrence Livermore National Security, LLC
 * Description:   AMR communication tests for edge-centered patch data
 *
 ************************************************************************/

#include "EdgeDataTest.h"

#include "SAMRAI/pdat/ArrayData.h"
#include "SAMRAI/hier/BoundaryBox.h"
#include "SAMRAI/geom/CartesianPatchGeometry.h"
#include "SAMRAI/pdat/CellIndex.h"
#include "SAMRAI/pdat/CellIterator.h"
#include "CommTester.h"
#include "SAMRAI/pdat/CellGeometry.h"
#include "SAMRAI/pdat/EdgeGeometry.h"
#include "SAMRAI/pdat/EdgeIndex.h"
#include "SAMRAI/pdat/EdgeIterator.h"
#include "SAMRAI/pdat/EdgeVariable.h"
#include "SAMRAI/tbox/MathUtilities.h"
#include "SAMRAI/tbox/Utilities.h"
#include "SAMRAI/tbox/MathUtilities.h"
#include "SAMRAI/hier/VariableDatabase.h"

namespace SAMRAI {


static double GETVALUE(
   const int dim,
   const double a,
   const double b)
{
   return a * ((double)dim) + b;
}

EdgeDataTest::EdgeDataTest(
   const std::string& object_name,
   const tbox::Dimension& dim,
   std::shared_ptr<tbox::Database> main_input_db,
   bool do_refine,
   bool do_coarsen,
   const std::string& refine_option):
   PatchDataTestStrategy(dim),
   d_dim(dim)
{
   TBOX_ASSERT(!object_name.empty());
   TBOX_ASSERT(main_input_db);
   TBOX_ASSERT(!refine_option.empty());

   d_object_name = object_name;

   d_do_refine = do_refine;
   d_do_coarsen = false;
   if (!do_refine) {
      d_do_coarsen = do_coarsen;
   }

   d_refine_option = refine_option;

   d_use_fine_value_at_interface.resize(0);

   d_Acoef = 0.0;
   d_Bcoef = 0.0;
   d_Ccoef = 0.0;
   d_Dcoef = 0.0;

   d_finest_level_number = main_input_db->
      getDatabase("PatchHierarchy")->
      getInteger("max_levels") - 1;

   d_cart_grid_geometry.reset(
      new geom::CartesianGridGeometry(
         dim,
         "CartesianGridGeometry",
         main_input_db->getDatabase("CartesianGridGeometry")));

   setGridGeometry(d_cart_grid_geometry);

   readTestInput(main_input_db->getDatabase("EdgePatchDataTest"));

}

EdgeDataTest::~EdgeDataTest()
{
}

void EdgeDataTest::readTestInput(
   std::shared_ptr<tbox::Database> db)
{
   TBOX_ASSERT(db);

   /*
    * Read coeeficients of linear profile to test interpolation.
    */
   if (db->keyExists("Acoef")) {
      d_Acoef = db->getDouble("Acoef");
   } else {
      TBOX_ERROR(d_object_name << " input error: No `Acoeff' found." << std::endl);
   }
   if (db->keyExists("Dcoef")) {
      d_Dcoef = db->getDouble("Dcoef");
   } else {
      TBOX_ERROR(d_object_name << " input error: No `Dcoef' found." << std::endl);
   }
   if (d_dim > tbox::Dimension(1)) {
      if (db->keyExists("Bcoef")) {
         d_Bcoef = db->getDouble("Bcoef");
      } else {
         TBOX_ERROR(d_object_name << " input error: No `Bcoef' found." << std::endl);
      }
   }
   if (d_dim > tbox::Dimension(2)) {
      if (db->keyExists("Ccoef")) {
         d_Ccoef = db->getDouble("Ccoef");
      } else {
         TBOX_ERROR(d_object_name << " input error: No `Ccoef' found." << std::endl);
      }
   }

   /*
    * Base class reads variable parameters and boxes to refine.
    */

   readVariableInput(db->getDatabase("VariableData"));

   std::shared_ptr<tbox::Database> var_data(
      db->getDatabase("VariableData"));
   std::vector<std::string> var_keys = var_data->getAllKeys();
   int nkeys = static_cast<int>(var_keys.size());

   d_use_fine_value_at_interface.resize(nkeys);

   for (int i = 0; i < nkeys; ++i) {
      std::shared_ptr<tbox::Database> var_db(
         var_data->getDatabase(var_keys[i]));

      if (var_db->keyExists("use_fine_value_at_interface")) {
         d_use_fine_value_at_interface[i] =
            var_db->getBool("use_fine_value_at_interface");
      } else {
         d_use_fine_value_at_interface[i] = true;
      }

   }

}

void EdgeDataTest::registerVariables(
   CommTester* commtest)
{
   TBOX_ASSERT(commtest != 0);

   int nvars = static_cast<int>(d_variable_src_name.size());

   d_variables.resize(nvars);

   for (int i = 0; i < nvars; ++i) {
      d_variables[i].reset(
         new pdat::EdgeVariable<EDGE_KERNEL_TYPE>(d_dim, d_variable_src_name[i],
            d_variable_depth[i],
            d_use_fine_value_at_interface[i]));

      if (d_do_refine) {
         commtest->registerVariable(d_variables[i],
            d_variables[i],
            d_variable_src_ghosts[i],
            d_variable_dst_ghosts[i],
            d_cart_grid_geometry,
            d_variable_refine_op[i]);
      } else if (d_do_coarsen) {
         commtest->registerVariable(d_variables[i],
            d_variables[i],
            d_variable_src_ghosts[i],
            d_variable_dst_ghosts[i],
            d_cart_grid_geometry,
            d_variable_coarsen_op[i]);
      }

   }

}

void EdgeDataTest::setConstantData(
   std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> > data,
   const hier::Box& box,
   double ndimfact,
   double axfact) const
{
   NULL_USE(axfact);

   TBOX_ASSERT(data);

   if (!box.empty()) {

      for (int axis = 0; axis < d_dim.getValue(); ++axis) {
         pdat::ArrayData<EDGE_KERNEL_TYPE>& array = data->getArrayData(axis);

         double value = GETVALUE(d_dim.getValue(), ndimfact, axis);
         hier::Box edge_box = pdat::EdgeGeometry::toEdgeBox(box, axis);
         array.fillAll(value, edge_box);
      }
   }

}

void EdgeDataTest::setConservativeData(
   std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> > data,
   const hier::Box& box,
   const hier::Patch& patch,
   const std::shared_ptr<hier::PatchHierarchy> hierarchy,
   int level_number) const
{
   TBOX_ASSERT(data);
   TBOX_ASSERT(hierarchy);
   TBOX_ASSERT((level_number >= 0)
      && (level_number <= hierarchy->getFinestLevelNumber()));

   int i, j;
   std::shared_ptr<hier::PatchLevel> level(
      hierarchy->getPatchLevel(level_number));

   const hier::BoxContainer& domain =
      level->getPhysicalDomain(hier::BlockId::zero());
   size_t ncells = 0;
   for (hier::BoxContainer::const_iterator i = domain.begin();
        i != domain.end(); ++i) {
      ncells += i->size();
   }

   const int depth = data->getDepth();

   const hier::Box sbox = data->getGhostBox() * box;

   if (level_number == 0) {

      /*
       * Set edge value on level zero as follows:
       *
       *    u0(i,j,k) = (j + k)/ncells
       *    u1(i,j,k) = (i + k)/ncells
       *    u2(i,j,k) = (i + j)/ncells
       */

      for (int axis = 0; axis < d_dim.getValue(); ++axis) {
         pdat::CellIterator ciend(pdat::CellGeometry::end(sbox));
         for (pdat::CellIterator ci(pdat::CellGeometry::begin(sbox));
              ci != ciend; ++ci) {
            double value = 0.0;
            for (i = 0; i < d_dim.getValue(); ++i) {
               if (i == axis) {
                  value += (double)((*ci)(i));
               }
            }
            value /= static_cast<double>(ncells);
            if (d_dim == tbox::Dimension(1)) {
               for (int edge = 0; edge < 1; ++edge) {
                  pdat::EdgeIndex si(*ci, axis, edge);
                  for (int d = 0; d < depth; ++d) {
                     (*data)(si, d) = value;
                  }
               }
            }
            if (d_dim == tbox::Dimension(2)) {
               for (int edge = pdat::EdgeIndex::Lower;
                    edge <= pdat::EdgeIndex::Upper;
                    ++edge) {
                  pdat::EdgeIndex si(*ci, axis, edge);
                  for (int d = 0; d < depth; ++d) {
                     (*data)(si, d) = value;
                  }
               }
            }
            if (d_dim == tbox::Dimension(3)) {
               for (int edge = pdat::EdgeIndex::LowerLeft;
                    edge <= pdat::EdgeIndex::UpperRight;
                    ++edge) {
                  pdat::EdgeIndex si(*ci, axis, edge);
                  for (int d = 0; d < depth; ++d) {
                     (*data)(si, d) = value;
                  }
               }
            }
         }
      }

   } else {

      /*
       * Set edge value on level > 0 to
       *    u(i,j,k) = u_c + ci*del_i + cj*del_j + ck*del_k
       * where u_c is value on the underlying coarse edge, (ci,cj,ck) is
       * the underlying coarse edge index, and (del_i,del_j,del_k)
       * is the vector between the coarse and fine cell edge centers.
       */

      hier::IntVector ratio(level->getRatioToLevelZero());
      const int max_ratio = ratio.max();

      std::shared_ptr<geom::CartesianPatchGeometry> pgeom(
         SAMRAI_SHARED_PTR_CAST<geom::CartesianPatchGeometry, hier::PatchGeometry>(
            patch.getPatchGeometry()));
      TBOX_ASSERT(pgeom);
      const double* dx = pgeom->getDx();

      size_t coarse_ncells = static_cast<size_t>(ncells);
      double* delta = new double[max_ratio * d_dim.getValue()];
      for (j = 0; j < d_dim.getValue(); ++j) {
         coarse_ncells /= ratio(j);
         double coarse_dx = dx[j] * ratio(j);
         for (i = 0; i < ratio(j); ++i) {
            delta[j * max_ratio + i] = (i + 0.5) * dx[j] - coarse_dx * 0.5;
         }
      }

      for (int axis = 0; axis < d_dim.getValue(); ++axis) {
         hier::IntVector ci(ratio.getDim());
         hier::IntVector del(ratio.getDim());
         pdat::CellIterator fiend(pdat::CellGeometry::end(sbox));
         for (pdat::CellIterator fi(pdat::CellGeometry::begin(sbox));
              fi != fiend; ++fi) {
            double value = 0.0;
            for (i = 0; i < d_dim.getValue(); ++i) {
               if (i == axis) {
                  int findx = (*fi)(i);
                  ci(i) = ((findx < 0) ? (findx + 1) / ratio(i) - 1
                           : findx / ratio(i));
                  del(i) = (int)delta[i * max_ratio + findx - ci(i) * ratio(i)];
                  value += (double)(ci(i));
               }
            }
            value /= static_cast<double>(coarse_ncells);

            for (j = 0; j < d_dim.getValue(); ++j) {
               if (j == axis) {
                  value += ci(j) * del(j);
               }
            }

            if (d_dim == tbox::Dimension(1)) {
               for (int edge = 0; edge < 1; ++edge) {
                  pdat::EdgeIndex si(*fi, axis, edge);
                  for (int d = 0; d < depth; ++d) {
                     (*data)(si, d) = value;
                  }
               }
            }
            if (d_dim == tbox::Dimension(2)) {
               for (int edge = pdat::EdgeIndex::Lower;
                    edge <= pdat::EdgeIndex::Upper;
                    ++edge) {
                  pdat::EdgeIndex si(*fi, axis, edge);
                  for (int d = 0; d < depth; ++d) {
                     (*data)(si, d) = value;
                  }
               }
            }
            if (d_dim == tbox::Dimension(3)) {
               for (int edge = pdat::EdgeIndex::LowerLeft;
                    edge <= pdat::EdgeIndex::UpperRight;
                    ++edge) {
                  pdat::EdgeIndex si(*fi, axis, edge);
                  for (int d = 0; d < depth; ++d) {
                     (*data)(si, d) = value;
                  }
               }
            }

         }
      }
      delete[] delta;

   }

}

void EdgeDataTest::initializeDataOnPatch(
   const hier::Patch& patch,
   const std::shared_ptr<hier::PatchHierarchy> hierarchy,
   int level_number,
   char src_or_dst)
{
   NULL_USE(src_or_dst);

   d_hierarchy = hierarchy;

   if (d_do_refine) {

      for (int i = 0; i < static_cast<int>(d_variables.size()); ++i) {

         std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> > edge_data(
            SAMRAI_SHARED_PTR_CAST<pdat::EdgeData<EDGE_KERNEL_TYPE>, hier::PatchData>(
               patch.getPatchData(d_variables[i], getDataContext())));
         TBOX_ASSERT(edge_data);

         hier::Box dbox = edge_data->getBox();

         setLinearData(edge_data, dbox,
            patch);
      }

   } else if (d_do_coarsen) {

      for (int i = 0; i < static_cast<int>(d_variables.size()); ++i) {

         std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> > edge_data(
            SAMRAI_SHARED_PTR_CAST<pdat::EdgeData<EDGE_KERNEL_TYPE>, hier::PatchData>(
               patch.getPatchData(d_variables[i], getDataContext())));
         TBOX_ASSERT(edge_data);

         hier::Box dbox = edge_data->getGhostBox();

         setConservativeData(edge_data, dbox,
            patch, hierarchy, level_number);

      }

   }

}

void EdgeDataTest::setConstantBoundaryData(
   std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> > data,
   const hier::BoundaryBox& bbox,
   double ndimfact,
   double axfact) const
{
   TBOX_ASSERT(data);

   int lid = bbox.getLocationIndex();
   const hier::IntVector& gcw(data->getGhostCellWidth());
   hier::Box fillbox = bbox.getBox();

   if (bbox.getBoundaryType() == d_dim.getValue()) {

      for (tbox::Dimension::dir_t axis = 0; axis < d_dim.getValue(); ++axis) {
         if (axis == 0) {
            if (lid % 2) {
               fillbox.growLower(axis,
                  tbox::MathUtilities<int>::Max(gcw(axis) - 1, 0));
            } else {
               fillbox.growUpper(axis,
                  tbox::MathUtilities<int>::Max(gcw(axis) - 1, 0));
            }
         }
         if (axis == 1) {
            if ((lid == 0) || (lid == 1) || (lid == 4) || (lid == 5)) {
               fillbox.growLower(axis,
                  tbox::MathUtilities<int>::Max(gcw(axis) - 1, 0));
            } else {
               fillbox.growUpper(axis,
                  tbox::MathUtilities<int>::Max(gcw(axis) - 1, 0));
            }
         }
         if (axis == 2) {
            if (lid < 4) {
               fillbox.growLower(axis,
                  tbox::MathUtilities<int>::Max(gcw(axis) - 1, 0));
            } else {
               fillbox.growUpper(axis,
                  tbox::MathUtilities<int>::Max(gcw(axis) - 1, 0));
            }
         }
      }

   } else if (bbox.getBoundaryType() == 1) {
      for (tbox::Dimension::dir_t axis = 0; axis < d_dim.getValue(); ++axis) {
         if (lid == 2 * axis) {
            fillbox.growLower(axis,
               tbox::MathUtilities<int>::Max(gcw(axis) - 1, 0));
         } else if (lid == 2 * axis + 1) {
            fillbox.growUpper(axis,
               tbox::MathUtilities<int>::Max(gcw(axis) - 1, 0));
         }
      }
   } else if (bbox.getBoundaryType() == 2) {
      switch (lid) {
         case 0: {
            fillbox.growLower(1, tbox::MathUtilities<int>::Max(gcw(1) - 1, 0));
            fillbox.growLower(2, tbox::MathUtilities<int>::Max(gcw(2) - 1, 0));
            break;
         }
         case 1: {
            fillbox.growUpper(1, tbox::MathUtilities<int>::Max(gcw(1) - 1, 0));
            fillbox.growLower(2, tbox::MathUtilities<int>::Max(gcw(2) - 1, 0));
            break;
         }
         case 2: {
            fillbox.growLower(1, tbox::MathUtilities<int>::Max(gcw(1) - 1, 0));
            fillbox.growUpper(2, tbox::MathUtilities<int>::Max(gcw(2) - 1, 0));
            break;
         }
         case 3: {
            fillbox.growUpper(1, tbox::MathUtilities<int>::Max(gcw(1) - 1, 0));
            fillbox.growUpper(2, tbox::MathUtilities<int>::Max(gcw(2) - 1, 0));
            break;
         }
         case 4: {
            fillbox.growLower(0, tbox::MathUtilities<int>::Max(gcw(0) - 1, 0));
            fillbox.growLower(2, tbox::MathUtilities<int>::Max(gcw(2) - 1, 0));
            break;
         }
         case 5: {
            fillbox.growLower(0, tbox::MathUtilities<int>::Max(gcw(0) - 1, 0));
            fillbox.growUpper(2, tbox::MathUtilities<int>::Max(gcw(2) - 1, 0));
            break;
         }
         case 6: {
            fillbox.growUpper(0, tbox::MathUtilities<int>::Max(gcw(0) - 1, 0));
            fillbox.growLower(2, tbox::MathUtilities<int>::Max(gcw(2) - 1, 0));
            break;
         }
         case 7: {
            fillbox.growUpper(0, tbox::MathUtilities<int>::Max(gcw(0) - 1, 0));
            fillbox.growUpper(2, tbox::MathUtilities<int>::Max(gcw(2) - 1, 0));
            break;
         }
         case 8: {
            fillbox.growLower(0, tbox::MathUtilities<int>::Max(gcw(0) - 1, 0));
            fillbox.growLower(1, tbox::MathUtilities<int>::Max(gcw(1) - 1, 0));
            break;
         }
         case 9: {
            fillbox.growUpper(0, tbox::MathUtilities<int>::Max(gcw(0) - 1, 0));
            fillbox.growLower(1, tbox::MathUtilities<int>::Max(gcw(1) - 1, 0));
            break;
         }
         case 10: {
            fillbox.growLower(0, tbox::MathUtilities<int>::Max(gcw(0) - 1, 0));
            fillbox.growUpper(1, tbox::MathUtilities<int>::Max(gcw(1) - 1, 0));
            break;
         }
         case 11: {
            fillbox.growUpper(0, tbox::MathUtilities<int>::Max(gcw(0) - 1, 0));
            fillbox.growUpper(1, tbox::MathUtilities<int>::Max(gcw(1) - 1, 0));
            break;
         }
      }
   }

   for (int id = 0; id < d_dim.getValue(); ++id) {
      double value = GETVALUE(d_dim.getValue(), ndimfact, axfact);
      data->getArrayData(id).fillAll(value,
         pdat::EdgeGeometry::toEdgeBox(fillbox, id));
   }

}

/*
 *************************************************************************
 *
 * Verify results of communication operations.  This test must be
 * consistent with data initialization and boundary operations above.
 *
 *************************************************************************
 */

bool EdgeDataTest::verifyResults(
   const hier::Patch& patch,
   const std::shared_ptr<hier::PatchHierarchy> hierarchy,
   int level_number)
{
   bool test_failed = false;
   if (d_dim <= tbox::Dimension(1)) {
      return !test_failed;
   }
   if (d_do_refine || d_do_coarsen) {

      tbox::plog << "\nEntering EdgeDataTest::verifyResults..." << std::endl;
      tbox::plog << "level_number = " << level_number << std::endl;
      tbox::plog << "Patch box = " << patch.getBox() << std::endl;

      hier::IntVector tgcw(d_dim, 0);
      for (int i = 0; i < static_cast<int>(d_variables.size()); ++i) {
         tgcw.max(patch.getPatchData(d_variables[i], getDataContext())->
            getGhostCellWidth());
      }
      hier::Box pbox = patch.getBox();

      std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> > solution(
         new pdat::EdgeData<EDGE_KERNEL_TYPE>(pbox, 1, tgcw));

      hier::Box tbox(pbox);
      tbox.grow(tgcw);

      if (d_do_coarsen) {
         setConservativeData(solution, tbox,
            patch, hierarchy, level_number);
      }

      for (int i = 0; i < static_cast<int>(d_variables.size()); ++i) {

         std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> > edge_data(
            SAMRAI_SHARED_PTR_CAST<pdat::EdgeData<EDGE_KERNEL_TYPE>, hier::PatchData>(
               patch.getPatchData(d_variables[i], getDataContext())));
         TBOX_ASSERT(edge_data);
         int depth = edge_data->getDepth();
         hier::Box dbox = edge_data->getGhostBox();

         if (d_do_refine) {

            setLinearData(solution, tbox, patch);
         }

         for (int id = 0; id < d_dim.getValue(); ++id) {
            pdat::EdgeIterator siend(pdat::EdgeGeometry::end(dbox, id));
            for (pdat::EdgeIterator si(pdat::EdgeGeometry::begin(dbox, id));
                 si != siend; ++si) {
               EDGE_KERNEL_TYPE correct = (*solution)(*si);
               for (int d = 0; d < depth; ++d) {
                  EDGE_KERNEL_TYPE result = (*edge_data)(*si, d);
                  if (!tbox::MathUtilities<EDGE_KERNEL_TYPE>::equalEps(correct,
                         result)) {
                     test_failed = true;
                     tbox::perr << "Test FAILED: ...."
                                << " : edge_data index = " << *si << std::endl;
                     tbox::perr << "    hier::Variable = "
                                << d_variable_src_name[i]
                                << " : depth index = " << d << std::endl;
                     tbox::perr << "    result = " << result
                                << " : correct = " << correct << std::endl;
                  }
               }
            }
         }

      }

      solution.reset();   // just to be anal...

      tbox::plog << "\nExiting EdgeDataTest::verifyResults..." << std::endl;
      tbox::plog << "level_number = " << level_number << std::endl;
      tbox::plog << "Patch box = " << patch.getBox() << std::endl << std::endl;

   }

   return !test_failed;
}

void EdgeDataTest::setLinearData(
   std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> > data,
   const hier::Box& box,
   const hier::Patch& patch) const
{
   TBOX_ASSERT(data);

   std::shared_ptr<geom::CartesianPatchGeometry> pgeom(
      SAMRAI_SHARED_PTR_CAST<geom::CartesianPatchGeometry, hier::PatchGeometry>(
         patch.getPatchGeometry()));
   TBOX_ASSERT(pgeom);
   const double* dx = pgeom->getDx();
   const double* lowerx = pgeom->getXLower();
   double x, y, z;

   const int depth = data->getDepth();

   const hier::Box sbox = data->getGhostBox() * box;

   for (int axis = 0; axis < d_dim.getValue(); ++axis) {
      const pdat::EdgeIndex loweri(patch.getBox().lower(), axis, 0);
      pdat::EdgeIterator eiend(pdat::EdgeGeometry::end(sbox, axis));
      for (pdat::EdgeIterator ei(pdat::EdgeGeometry::begin(sbox, axis));
           ei != eiend; ++ei) {

         /*
          * Compute spatial location of cell center and
          * set data to linear profile.
          */

         if (axis != 0) {
            x = lowerx[0] + dx[0] * ((*ei)(0) - loweri(0));
         } else {
            x = lowerx[0] + dx[0] * ((*ei)(0) - loweri(0) + 0.5);
         }
         y = z = 0.;
         if (d_dim > tbox::Dimension(1)) {
            if (axis != 1) {
               y = lowerx[1] + dx[1] * ((*ei)(1) - loweri(1));
            } else {
               y = lowerx[1] + dx[1] * ((*ei)(1) - loweri(1) + 0.5);
            }
         }
         if (d_dim > tbox::Dimension(2)) {
            if (axis != 2) {
               z = lowerx[2] + dx[2] * ((*ei)(2) - loweri(2));
            } else {
               z = lowerx[2] + dx[2] * ((*ei)(2) - loweri(2) + 0.5);
            }
         }

         for (int d = 0; d < depth; ++d) {
            (*data)(*ei,
                    d) = d_Dcoef + d_Acoef * x + d_Bcoef * y + d_Ccoef * z;
         }

      }
   }

}

void EdgeDataTest::checkPatchInteriorData(
   const std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> >& data,
   const hier::Box& interior,
   const std::shared_ptr<geom::CartesianPatchGeometry>& pgeom) const
{
   TBOX_ASSERT(data);

   const double* dx = pgeom->getDx();
   const double* lowerx = pgeom->getXLower();
   double x, y, z;

   const int depth = data->getDepth();

   for (int axis = 0; axis < d_dim.getValue(); ++axis) {
      const pdat::EdgeIndex loweri(interior.lower(), axis, 0);
      pdat::EdgeIterator eiend(pdat::EdgeGeometry::end(interior, axis));
      for (pdat::EdgeIterator ei(pdat::EdgeGeometry::begin(interior, axis));
           ei != eiend; ++ei) {

         /*
          * Compute spatial location of cell center and
          * set data to linear profile.
          */

         if (axis != 0) {
            x = lowerx[0] + dx[0] * ((*ei)(0) - loweri(0));
         } else {
            x = lowerx[0] + dx[0] * ((*ei)(0) - loweri(0) + 0.5);
         }
         y = z = 0.;
         if (d_dim > tbox::Dimension(1)) {
            if (axis != 1) {
               y = lowerx[1] + dx[1] * ((*ei)(1) - loweri(1));
            } else {
               y = lowerx[1] + dx[1] * ((*ei)(1) - loweri(1) + 0.5);
            }
         }
         if (d_dim > tbox::Dimension(2)) {
            if (axis != 2) {
               z = lowerx[2] + dx[2] * ((*ei)(2) - loweri(2));
            } else {
               z = lowerx[2] + dx[2] * ((*ei)(2) - loweri(2) + 0.5);
            }
         }

         EDGE_KERNEL_TYPE value;
         for (int d = 0; d < depth; ++d) {
            value = d_Dcoef + d_Acoef * x + d_Bcoef * y + d_Ccoef * z;
            if (!(tbox::MathUtilities<EDGE_KERNEL_TYPE>::equalEps((*data)(*ei,
                                                                d), value))) {
               tbox::perr
               << "FAILED:  -- patch interior not properly filled" << std::endl;
            }
         }

      }
   }

}

void EdgeDataTest::setPhysicalBoundaryConditions(
   const hier::Patch& patch,
   const double time,
   const hier::IntVector& gcw) const
{
   NULL_USE(time);

   std::shared_ptr<geom::CartesianPatchGeometry> pgeom(
      SAMRAI_SHARED_PTR_CAST<geom::CartesianPatchGeometry, hier::PatchGeometry>(
         patch.getPatchGeometry()));
   TBOX_ASSERT(pgeom);

   const std::vector<hier::BoundaryBox>& node_bdry =
      pgeom->getCodimensionBoundaries(d_dim.getValue());
   const int num_node_bdry_boxes = static_cast<int>(node_bdry.size());

   std::vector<hier::BoundaryBox> empty_vector(0, hier::BoundaryBox(d_dim));
   const std::vector<hier::BoundaryBox>& edge_bdry =
      d_dim > tbox::Dimension(1) ?
      pgeom->getCodimensionBoundaries(d_dim.getValue() - 1) : empty_vector;
   const int num_edge_bdry_boxes = static_cast<int>(edge_bdry.size());

   const std::vector<hier::BoundaryBox>& face_bdry =
      d_dim == tbox::Dimension(3) ?
      pgeom->getCodimensionBoundaries(d_dim.getValue() - 2) : empty_vector;
   const int num_face_bdry_boxes = static_cast<int>(face_bdry.size());

   for (int i = 0; i < static_cast<int>(d_variables.size()); ++i) {

      std::shared_ptr<pdat::EdgeData<EDGE_KERNEL_TYPE> > edge_data(
         SAMRAI_SHARED_PTR_CAST<pdat::EdgeData<EDGE_KERNEL_TYPE>, hier::PatchData>(
            patch.getPatchData(d_variables[i], getDataContext())));
      TBOX_ASSERT(edge_data);

      hier::Box patch_interior = edge_data->getBox();
#if defined(HAVE_CUDA)
      cudaDeviceSynchronize();
#endif
      checkPatchInteriorData(edge_data, patch_interior, pgeom);

      /*
       * Set node boundary data.
       */
      for (int ni = 0; ni < num_node_bdry_boxes; ++ni) {
         hier::Box fill_box = pgeom->getBoundaryFillBox(node_bdry[ni],
               patch.getBox(),
               gcw);

         setLinearData(edge_data, fill_box, patch);
      }

      if (d_dim > tbox::Dimension(1)) {
         /*
          * Set edge boundary data.
          */
         for (int ei = 0; ei < num_edge_bdry_boxes; ++ei) {
            hier::Box fill_box = pgeom->getBoundaryFillBox(edge_bdry[ei],
                  patch.getBox(),
                  gcw);

            setLinearData(edge_data, fill_box, patch);
         }
      }

      if (d_dim == tbox::Dimension(3)) {
         /*
          * Set face boundary data.
          */
         for (int fi = 0; fi < num_face_bdry_boxes; ++fi) {
            hier::Box fill_box = pgeom->getBoundaryFillBox(face_bdry[fi],
                  patch.getBox(),
                  gcw);

            setLinearData(edge_data, fill_box, patch);
         }
      }

   }

}

}
