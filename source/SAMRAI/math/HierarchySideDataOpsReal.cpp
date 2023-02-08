/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and LICENSE.
 *
 * Copyright:     (c) 1997-2023 Lawrence Livermore National Security, LLC
 * Description:   Templated operations for real side data on multiple levels.
 *
 ************************************************************************/

#ifndef included_math_HierarchySideDataOpsReal_C
#define included_math_HierarchySideDataOpsReal_C

#include "SAMRAI/math/HierarchySideDataOpsReal.h"
#include "SAMRAI/hier/BoxUtilities.h"
#include "SAMRAI/hier/PatchDescriptor.h"
#include "SAMRAI/pdat/SideDataFactory.h"
#include "SAMRAI/tbox/SAMRAI_MPI.h"
#include "SAMRAI/tbox/MathUtilities.h"
#include <typeinfo>
#include <stdlib.h>
#include <float.h>
#include <math.h>

namespace SAMRAI {
namespace math {

template<class TYPE>
HierarchySideDataOpsReal<TYPE>::HierarchySideDataOpsReal(
   const std::shared_ptr<hier::PatchHierarchy>& hierarchy,
   const int coarsest_level,
   const int finest_level):
   HierarchyDataOpsReal<TYPE>(),
   d_hierarchy(hierarchy)
{
   TBOX_ASSERT(hierarchy);

   if ((coarsest_level < 0) || (finest_level < 0)) {
      if (d_hierarchy->getNumberOfLevels() == 0) {
         d_coarsest_level = coarsest_level;
         d_finest_level = finest_level;
      } else {
         resetLevels(0, d_hierarchy->getFinestLevelNumber());
      }
   } else {
      resetLevels(coarsest_level, finest_level);
   }
}

template<class TYPE>
HierarchySideDataOpsReal<TYPE>::~HierarchySideDataOpsReal()
{
}

/*
 *************************************************************************
 *
 * Routines to set the hierarchy and level informtation.
 *
 *************************************************************************
 */

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::setPatchHierarchy(
   const std::shared_ptr<hier::PatchHierarchy>& hierarchy)
{
   TBOX_ASSERT(hierarchy);

   d_hierarchy = hierarchy;
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::resetLevels(
   const int coarsest_level,
   const int finest_level)
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((coarsest_level >= 0)
      && (finest_level >= coarsest_level)
      && (finest_level <= d_hierarchy->getFinestLevelNumber()));

   int dim_val = d_hierarchy->getDim().getValue();

   d_coarsest_level = coarsest_level;
   d_finest_level = finest_level;

   for (int d = 0; d < dim_val; ++d) {
      d_nonoverlapping_side_boxes[d].resize(d_finest_level + 1);
   }

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      hier::BoxContainer side_boxes;

      for (tbox::Dimension::dir_t nd = 0; nd < dim_val; ++nd) {
         side_boxes = level->getBoxes();
         for (hier::BoxContainer::iterator i = side_boxes.begin();
              i != side_boxes.end(); ++i) {
            *i = pdat::SideGeometry::toSideBox(*i, nd);
         }
         hier::BoxUtilities::makeNonOverlappingBoxContainers(
            d_nonoverlapping_side_boxes[nd][ln],
            side_boxes);
      }
   }
}

template<class TYPE>
const std::shared_ptr<hier::PatchHierarchy>
HierarchySideDataOpsReal<TYPE>::getPatchHierarchy() const
{
   return d_hierarchy;
}

/*
 *************************************************************************
 *
 * Basic generic operations.
 *
 *************************************************************************
 */

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::copyData(
   const int dst_id,
   const int src_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.copyData(dst, src, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::swapData(
   const int data1_id,
   const int data2_id) const
{
#ifdef DEBUG_CHECK_ASSERTIONS
   std::shared_ptr<pdat::SideDataFactory<TYPE> > d1fact(
      SAMRAI_SHARED_PTR_CAST<pdat::SideDataFactory<TYPE>, hier::PatchDataFactory>(
         d_hierarchy->getPatchDescriptor()->getPatchDataFactory(data1_id)));
   TBOX_ASSERT(d1fact);
   std::shared_ptr<pdat::SideDataFactory<TYPE> > d2fact(
      SAMRAI_SHARED_PTR_CAST<pdat::SideDataFactory<TYPE>, hier::PatchDataFactory>(
         d_hierarchy->getPatchDescriptor()->getPatchDataFactory(data2_id)));
   TBOX_ASSERT(d2fact);
   TBOX_ASSERT(d1fact->getDepth() == d2fact->getDepth());
   TBOX_ASSERT(d1fact->getGhostCellWidth() == d2fact->getGhostCellWidth());
   TBOX_ASSERT(d1fact->getDirectionVector() == d2fact->getDirectionVector());
#endif

   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         d_patch_ops.swapData(p, data1_id, data2_id);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::printData(
   const int data_id,
   std::ostream& s,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   auto factory = d_hierarchy->getPatchDescriptor()->
      getPatchDataFactory(data_id).get();
   s << "Patch descriptor id = " << data_id << std::endl;
   s << "Factory = " << typeid(*factory).name()
     << std::endl;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      s << "Level number = " << ln << std::endl;
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > d(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));

         TBOX_ASSERT(d);

         hier::Box box = (interior_only ? p->getBox() : d->getGhostBox());

         d_patch_ops.printData(d, box, s);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::setToScalar(
   const int data_id,
   const TYPE& alpha,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > d(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));

         TBOX_ASSERT(d);

         hier::Box box = (interior_only ? p->getBox() : d->getGhostBox());

         d_patch_ops.setToScalar(d, alpha, box);
      }
   }
}

/*
 *************************************************************************
 *
 * Basic generic arithmetic operations.
 *
 *************************************************************************
 */

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::scale(
   const int dst_id,
   const TYPE& alpha,
   const int src_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.scale(dst, alpha, src, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::addScalar(
   const int dst_id,
   const int src_id,
   const TYPE& alpha,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.addScalar(dst, src, alpha, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::add(
   const int dst_id,
   const int src1_id,
   const int src2_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src1(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src1_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src2(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src2_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src1);
         TBOX_ASSERT(src2);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.add(dst, src1, src2, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::subtract(
   const int dst_id,
   const int src1_id,
   const int src2_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src1(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src1_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src2(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src2_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src1);
         TBOX_ASSERT(src2);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.subtract(dst, src1, src2, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::multiply(
   const int dst_id,
   const int src1_id,
   const int src2_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src1(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src1_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src2(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src2_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src1);
         TBOX_ASSERT(src2);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.multiply(dst, src1, src2, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::divide(
   const int dst_id,
   const int src1_id,
   const int src2_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src1(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src1_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src2(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src2_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src1);
         TBOX_ASSERT(src2);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.divide(dst, src1, src2, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::reciprocal(
   const int dst_id,
   const int src_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.reciprocal(dst, src, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::linearSum(
   const int dst_id,
   const TYPE& alpha,
   const int src1_id,
   const TYPE& beta,
   const int src2_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src1(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src1_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src2(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src2_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src1);
         TBOX_ASSERT(src2);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.linearSum(dst, alpha, src1, beta, src2, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::axpy(
   const int dst_id,
   const TYPE& alpha,
   const int src1_id,
   const int src2_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src1(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src1_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src2(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src2_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src1);
         TBOX_ASSERT(src2);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.axpy(dst, alpha, src1, src2, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::axmy(
   const int dst_id,
   const TYPE& alpha,
   const int src1_id,
   const int src2_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src1(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src1_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src2(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src2_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src1);
         TBOX_ASSERT(src2);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.axmy(dst, alpha, src1, src2, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::abs(
   const int dst_id,
   const int src_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src_id)));

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src);

         hier::Box box = (interior_only ? p->getBox() : dst->getGhostBox());

         d_patch_ops.abs(dst, src, box);
      }
   }
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::setRandomValues(
   const int data_id,
   const TYPE& width,
   const TYPE& low,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > data(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));

         TBOX_ASSERT(data);

         hier::Box box = (interior_only ? p->getBox() : data->getGhostBox());

         d_patch_ops.setRandomValues(data, width, low, box);
      }
   }
}

/*
 *************************************************************************
 *
 * Generic norm and order operations.
 *
 *************************************************************************
 */

template<class TYPE>
size_t
HierarchySideDataOpsReal<TYPE>::numberOfEntries(
   const int data_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));
   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());
   int dim_val = d_hierarchy->getDim().getValue();

   size_t entries = 0;

   if (interior_only) {

      std::shared_ptr<pdat::SideDataFactory<TYPE> > dfact(
         SAMRAI_SHARED_PTR_CAST<pdat::SideDataFactory<TYPE>, hier::PatchDataFactory>(
            d_hierarchy->getPatchDescriptor()->getPatchDataFactory(data_id)));

      TBOX_ASSERT(dfact);

      const hier::IntVector& directions = dfact->getDirectionVector();

      for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
         std::shared_ptr<hier::PatchLevel> level(
            d_hierarchy->getPatchLevel(ln));
         const int npatches = level->getNumberOfPatches();
#ifdef DEBUG_CHECK_ASSERTIONS
         for (int dc = 0; dc < dim_val; ++dc) {
            TBOX_ASSERT(npatches == static_cast<int>(d_nonoverlapping_side_boxes[dc][ln].size()));
         }
#endif
         for (int il = 0; il < npatches; ++il) {
            for (int eb = 0; eb < dim_val; ++eb) {
               if (directions(eb)) {
                  hier::BoxContainer::const_iterator lb =
                     ((d_nonoverlapping_side_boxes[eb][ln])[il]).begin();
                  for ( ; lb != ((d_nonoverlapping_side_boxes[eb][ln])[il]).end();
                        ++lb) {
                     entries += lb->size();
                  }
               }
            }
         }
      }

      entries *= dfact->getDepth();

   } else {

      for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
         std::shared_ptr<hier::PatchLevel> level(
            d_hierarchy->getPatchLevel(ln));
         for (hier::PatchLevel::iterator ip(level->begin());
              ip != level->end(); ++ip) {
            std::shared_ptr<pdat::SideData<TYPE> > d(
               SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
                  (*ip)->getPatchData(data_id)));

            TBOX_ASSERT(d);

            entries += d_patch_ops.numberOfEntries(d, d->getGhostBox());
         }
      }

      unsigned long int global_entries = entries;
      if (mpi.getSize() > 1) {
         mpi.Allreduce(&entries, &global_entries, 1, MPI_UNSIGNED_LONG, MPI_SUM);
      }
      entries = global_entries;

   }

   return entries;
}

template<class TYPE>
double
HierarchySideDataOpsReal<TYPE>::sumControlVolumes(
   const int data_id,
   const int vol_id) const
{
   TBOX_ASSERT(vol_id >= 0);
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   double sum = 0.0;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > data(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));
         std::shared_ptr<pdat::SideData<double> > cv(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<double>, hier::PatchData>(
               p->getPatchData(vol_id)));

         TBOX_ASSERT(data);
         TBOX_ASSERT(cv);

         hier::Box box = cv->getGhostBox();

         sum += d_patch_ops.sumControlVolumes(data, cv, box);
      }
   }

   double global_sum = sum;
   if (mpi.getSize() > 1) {
      mpi.Allreduce(&sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM);
   }
   return global_sum;
}

template<class TYPE>
double
HierarchySideDataOpsReal<TYPE>::L1Norm(
   const int data_id,
   const int vol_id,
   bool local_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   double norm = 0.0;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > data(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));
         std::shared_ptr<hier::PatchData> pd;

         TBOX_ASSERT(data);

         hier::Box box = p->getBox();
         if (vol_id >= 0) {

            box = data->getGhostBox();
            pd = p->getPatchData(vol_id);
         }

         std::shared_ptr<pdat::SideData<double> > cv(
            std::dynamic_pointer_cast<pdat::SideData<double>,
                                        hier::PatchData>(pd));
         norm += d_patch_ops.L1Norm(data, box, cv);
      }
   }

   if (!local_only) {
      double global_norm = norm;
      if (mpi.getSize() > 1) {
         mpi.Allreduce(&norm, &global_norm, 1, MPI_DOUBLE, MPI_SUM);
      }
      norm = global_norm;
   }
   return norm;
}

template<class TYPE>
double
HierarchySideDataOpsReal<TYPE>::L2Norm(
   const int data_id,
   const int vol_id,
   bool local_only) const
{
   double norm_squared = HierarchySideDataOpsReal<TYPE>::dot(data_id,
         data_id,
         vol_id,
         local_only);

   return sqrt(norm_squared);
}

template<class TYPE>
double
HierarchySideDataOpsReal<TYPE>::weightedL2Norm(
   const int data_id,
   const int wgt_id,
   const int vol_id) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   double norm_squared = 0.0;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > data(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));
         std::shared_ptr<pdat::SideData<TYPE> > weight(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(wgt_id)));
         std::shared_ptr<hier::PatchData> pd;

         TBOX_ASSERT(data);
         TBOX_ASSERT(weight);

         hier::Box box = p->getBox();
         if (vol_id >= 0) {

            box = data->getGhostBox();
            pd = p->getPatchData(vol_id);
         }

         std::shared_ptr<pdat::SideData<double> > cv(
            std::dynamic_pointer_cast<pdat::SideData<double>,
                                        hier::PatchData>(pd));
         double pnorm = d_patch_ops.weightedL2Norm(data, weight, box, cv);

         norm_squared += pnorm * pnorm;
      }
   }

   double global_norm_squared = norm_squared;
   if (mpi.getSize() > 1) {
      mpi.Allreduce(&norm_squared, &global_norm_squared, 1, MPI_DOUBLE, MPI_SUM);
   }
   return sqrt(global_norm_squared);
}

template<class TYPE>
double
HierarchySideDataOpsReal<TYPE>::RMSNorm(
   const int data_id,
   const int vol_id) const
{
   double l2_norm = L2Norm(data_id, vol_id);

   double volume = ((vol_id < 0) ? (double)numberOfEntries(data_id, true)
                    : sumControlVolumes(data_id, vol_id));

   double rms_norm = l2_norm / sqrt(volume);
   return rms_norm;
}

template<class TYPE>
double
HierarchySideDataOpsReal<TYPE>::weightedRMSNorm(
   const int data_id,
   const int wgt_id,
   const int vol_id) const
{

   double l2_norm = weightedL2Norm(data_id, wgt_id, vol_id);

   double volume = ((vol_id < 0) ? (double)numberOfEntries(data_id, true)
                    : sumControlVolumes(data_id, vol_id));

   double rms_norm = l2_norm / sqrt(volume);
   return rms_norm;
}

template<class TYPE>
double
HierarchySideDataOpsReal<TYPE>::maxNorm(
   const int data_id,
   const int vol_id,
   bool local_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   double norm = 0.0;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > data(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));
         std::shared_ptr<hier::PatchData> pd;

         TBOX_ASSERT(data);

         hier::Box box = p->getBox();
         if (vol_id >= 0) {

            box = data->getGhostBox();
            pd = p->getPatchData(vol_id);
         }

         std::shared_ptr<pdat::SideData<double> > cv(
            std::dynamic_pointer_cast<pdat::SideData<double>,
                                        hier::PatchData>(pd));
         norm = tbox::MathUtilities<double>::Max(norm,
               d_patch_ops.maxNorm(data, box, cv));
      }
   }

   if (!local_only) {
      double global_norm = norm;
      if (mpi.getSize() > 1) {
         mpi.Allreduce(&norm, &global_norm, 1, MPI_DOUBLE, MPI_MAX);
      }
      norm = global_norm;
   }
   return norm;
}

template<class TYPE>
TYPE
HierarchySideDataOpsReal<TYPE>::dot(
   const int data1_id,
   const int data2_id,
   const int vol_id,
   bool local_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   TYPE dprod = 0.0;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > data1(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data1_id)));
         std::shared_ptr<pdat::SideData<TYPE> > data2(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data2_id)));
         std::shared_ptr<hier::PatchData> pd;

         TBOX_ASSERT(data1);
         TBOX_ASSERT(data2);

         hier::Box box = p->getBox();
         if (vol_id >= 0) {

            box = data1->getGhostBox();
            pd = p->getPatchData(vol_id);
         }

         std::shared_ptr<pdat::SideData<double> > cv(
            std::dynamic_pointer_cast<pdat::SideData<double>,
                                        hier::PatchData>(pd));
         dprod += d_patch_ops.dot(data1, data2, box, cv);
      }
   }

   if (!local_only) {
      if (mpi.getSize() > 1) {
         mpi.AllReduce(&dprod, 1, MPI_SUM);
      }
   }
   return dprod;
}

template<class TYPE>
TYPE
HierarchySideDataOpsReal<TYPE>::integral(
   const int data_id,
   const int vol_id) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   TYPE local_integral = 0.0;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > data(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));
         std::shared_ptr<pdat::SideData<double> > vol(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<double>, hier::PatchData>(
               p->getPatchData(vol_id)));

         TBOX_ASSERT(data);
         TBOX_ASSERT(vol);

         hier::Box box = data->getGhostBox();

         local_integral += d_patch_ops.integral(data, box, vol);
      }
   }

   TYPE global_integral = local_integral;
   if (mpi.getSize() > 1) {
      mpi.AllReduce(&global_integral, 1, MPI_SUM);
   }
   return global_integral;
}

/*
 *************************************************************************
 *
 * Generic miscellaneous operations for real data.
 *
 *************************************************************************
 */

template<class TYPE>
int
HierarchySideDataOpsReal<TYPE>::computeConstrProdPos(
   const int data1_id,
   const int data2_id,
   const int vol_id) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   int test = 1;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > data1(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data1_id)));
         std::shared_ptr<pdat::SideData<TYPE> > data2(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data2_id)));
         std::shared_ptr<hier::PatchData> pd;

         TBOX_ASSERT(data1);
         TBOX_ASSERT(data2);

         hier::Box box = p->getBox();
         if (vol_id >= 0) {

            box = data1->getGhostBox();
            pd = p->getPatchData(vol_id);
         }

         std::shared_ptr<pdat::SideData<double> > cv(
            std::dynamic_pointer_cast<pdat::SideData<double>,
                                        hier::PatchData>(pd));
         test = tbox::MathUtilities<int>::Min(test,
               d_patch_ops.computeConstrProdPos(data1, data2, box, cv));
      }
   }

   int global_test = test;
   if (mpi.getSize() > 1) {
      mpi.Allreduce(&test, &global_test, 1, MPI_INT, MPI_MIN);
   }
   return global_test;
}

template<class TYPE>
void
HierarchySideDataOpsReal<TYPE>::compareToScalar(
   const int dst_id,
   const int src_id,
   const TYPE& alpha,
   const int vol_id) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src_id)));
         std::shared_ptr<hier::PatchData> pd;

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src);

         hier::Box box = p->getBox();
         if (vol_id >= 0) {

            box = dst->getGhostBox();
            pd = p->getPatchData(vol_id);
         }

         std::shared_ptr<pdat::SideData<double> > cv(
            std::dynamic_pointer_cast<pdat::SideData<double>,
                                        hier::PatchData>(pd));
         d_patch_ops.compareToScalar(dst, src, alpha, box, cv);
      }
   }
}

template<class TYPE>
int
HierarchySideDataOpsReal<TYPE>::testReciprocal(
   const int dst_id,
   const int src_id,
   const int vol_id) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   int test = 1;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > dst(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(dst_id)));
         std::shared_ptr<pdat::SideData<TYPE> > src(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(src_id)));
         std::shared_ptr<hier::PatchData> pd;

         TBOX_ASSERT(dst);
         TBOX_ASSERT(src);

         hier::Box box = p->getBox();
         if (vol_id >= 0) {

            box = dst->getGhostBox();
            pd = p->getPatchData(vol_id);
         }

         std::shared_ptr<pdat::SideData<double> > cv(
            std::dynamic_pointer_cast<pdat::SideData<double>,
                                        hier::PatchData>(pd));
         test = tbox::MathUtilities<int>::Min(test,
               d_patch_ops.testReciprocal(dst, src, box, cv));
      }
   }

   int global_test = test;
   if (mpi.getSize() > 1) {
      mpi.Allreduce(&test, &global_test, 1, MPI_INT, MPI_MIN);
   }
   return global_test;
}

template<class TYPE>
TYPE
HierarchySideDataOpsReal<TYPE>::maxPointwiseDivide(
   const int numer_id,
   const int denom_id,
   bool local_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   TYPE max = 0.0;

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > numer(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(numer_id)));
         std::shared_ptr<pdat::SideData<TYPE> > denom(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(denom_id)));

         TBOX_ASSERT(numer);
         TBOX_ASSERT(denom);

         hier::Box box = p->getBox();

         max = tbox::MathUtilities<TYPE>::Max(max,
               d_patch_ops.maxPointwiseDivide(numer, denom, box));
      }
   }

   if (!local_only) {
      if (mpi.getSize() > 1) {
         mpi.AllReduce(&max, 1, MPI_MAX);
      }
   }
   return max;
}

template<class TYPE>
TYPE
HierarchySideDataOpsReal<TYPE>::minPointwiseDivide(
   const int numer_id,
   const int denom_id,
   bool local_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   TYPE min = tbox::MathUtilities<TYPE>::getMax();

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > numer(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(numer_id)));
         std::shared_ptr<pdat::SideData<TYPE> > denom(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(denom_id)));

         TBOX_ASSERT(numer);
         TBOX_ASSERT(denom);

         hier::Box box = p->getBox();

         min = tbox::MathUtilities<TYPE>::Min(min,
               d_patch_ops.minPointwiseDivide(numer, denom, box));
      }
   }

   if (!local_only) {
      if (mpi.getSize() > 1) {
         mpi.AllReduce(&min, 1, MPI_MIN);
      }
   }
   return min;
}

template<class TYPE>
TYPE
HierarchySideDataOpsReal<TYPE>::min(
   const int data_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   TYPE minval = tbox::MathUtilities<TYPE>::getMax();

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > d(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));

         TBOX_ASSERT(d);

         hier::Box box = (interior_only ? p->getBox() : d->getGhostBox());

         minval = tbox::MathUtilities<TYPE>::Min(minval, d_patch_ops.min(d, box));
      }
   }

   TYPE global_min = minval;
   if (mpi.getSize() > 1) {
      mpi.AllReduce(&global_min, 1, MPI_MIN);
   }
   return global_min;
}

template<class TYPE>
TYPE
HierarchySideDataOpsReal<TYPE>::max(
   const int data_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   TYPE maxval = -tbox::MathUtilities<TYPE>::getMax();

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > d(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));

         TBOX_ASSERT(d);

         hier::Box box = (interior_only ? p->getBox() : d->getGhostBox());

         maxval = tbox::MathUtilities<TYPE>::Max(maxval, d_patch_ops.max(d, box));
      }
   }

   TYPE global_max = maxval;
   if (mpi.getSize() > 1) {
      mpi.AllReduce(&global_max, 1, MPI_MAX);
   }
   return global_max;
}

template<class TYPE>
int64_t HierarchySideDataOpsReal<TYPE>::getLength(
   const int data_id,
   const bool interior_only) const
{
   TBOX_ASSERT(d_hierarchy);
   TBOX_ASSERT((d_coarsest_level >= 0)
      && (d_finest_level >= d_coarsest_level)
      && (d_finest_level <= d_hierarchy->getFinestLevelNumber()));

   const tbox::SAMRAI_MPI& mpi(d_hierarchy->getMPI());

   int64_t length = 0;
   tbox::Dimension::dir_t dim_val = d_hierarchy->getDim().getValue();
   hier::Box data_box(d_hierarchy->getDim());

   for (int ln = d_coarsest_level; ln <= d_finest_level; ++ln) {
      std::shared_ptr<hier::PatchLevel> level(
         d_hierarchy->getPatchLevel(ln));
      for (hier::PatchLevel::iterator ip(level->begin());
           ip != level->end(); ++ip) {
         const std::shared_ptr<hier::Patch>& p = *ip;

         std::shared_ptr<pdat::SideData<TYPE> > data(
            SAMRAI_SHARED_PTR_CAST<pdat::SideData<TYPE>, hier::PatchData>(
               p->getPatchData(data_id)));

         const hier::IntVector& directions = data->getDirectionVector();

         TBOX_ASSERT(data);

         for (tbox::Dimension::dir_t d = 0; d < dim_val; ++d) {
            if (directions[d]) {
               if (interior_only) {
                  data_box = pdat::SideGeometry::toSideBox(data->getBox(), d);
               } else {
                  data_box = data->getArrayData(d).getBox();
               }
               length += static_cast<int64_t>(data_box.size()*data->getDepth());
            }
         }
      }
   }

   int64_t global_length = length;
   if (mpi.getSize() > 1) {
      mpi.Allreduce(&length, &global_length, 1, MPI_INT64_T, MPI_SUM);
   }
   return global_length;
}


}
}
#endif
