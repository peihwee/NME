#include <Graphics.h>
#include <set>

namespace nme
{

struct EdgePoint
{
   UserPoint p;
   EdgePoint      *next;
   EdgePoint      *prev;
   double Cross()
   {
      return (prev->p - p).Cross(next->p - p);
   }
   bool concave() { return(Cross()>0.0); }
   bool convex() { return(Cross()<0.0); }
};

// TODO: Merge this into EdgePoint
typedef std::set<EdgePoint *> EdgePointSet;

void Recalc(EdgePointSet &concave_points,EdgePoint *pi)
{
   EdgePointSet::iterator cp;
   if ( (cp=concave_points.find(pi))!=concave_points.end())
   {
      if ( !pi->concave() )
         concave_points.erase(cp);
   }
   else
   {
      if ( pi->concave() )
         concave_points.insert(pi);
   }
}

bool IsEar(const EdgePointSet &concave_points,EdgePoint *pi)
{
   EdgePoint *prev = pi->prev;

   UserPoint corner(prev->p);
   if (concave_points.find(prev)!=concave_points.end())
      return(false);

   UserPoint v1( pi->p - corner ); 
   UserPoint v2( prev->prev->p - corner );
   double denom = v1.Cross(v2);

   if (denom==0.0)  // flat triangle 
   {
      // Need to be consistent so we don't get opposite-sense triangles
      //  over the top of each other.
      // Don't worry about this here !
      return true;
   }


   for(EdgePointSet::const_iterator it = concave_points.begin();
          it!=concave_points.end(); ++it)
   {
      UserPoint v( (*it)->p-corner );
      double a = v.Cross(v2);
      double b = v1.Cross(v);
      if (a>=0.0 && b>=0.0 && (a+b)<denom && (a+b)>0)
         return false;
   }
   return(true);
}

bool Intersect(UserPoint dir,UserPoint p0,UserPoint p1)
{
   // Test for simple overlap first ?
   if (dir==p0 || dir==p1)
      return true;

   UserPoint v = p1-p0;
   double denom = dir.Cross(v);
   if (denom==0) // parallel - co-linear or not
   {
      if (p0.Cross(dir)!=0.0) // p0 is not on dir ...
         return false;

      // co-linear - find closest point on +ve direction on line ...
      double b[2];
      if (dir.x==0)
      {
         b[0] = (double)p0.y/(double)dir.y;
         b[1] = (double)p1.y/(double)dir.y;
      }
      else
      {
         b[0] = (double)p0.x/(double)dir.x;
         b[1] = (double)p1.x/(double)dir.x;
      }

      int point;
      if (b[0]>=0 && b[1]>=0)
         point = b[1]<b[0];
      else if (b[0]>=0)
         point = 0;
      else if (b[1]>=0)
         point = 1;
      else
         point = b[1]>b[0];
      
      return(b[point]>=0 && b[point]<=1.0);
   }

   double beta = p0.Cross(v)/denom;
   if (beta<0.0 || beta>1.0)
      return(false);

   // Test alpha ...
   double alpha = p0.Cross(dir)/denom;
   return(alpha>=0 && alpha<=1.0);
}

bool FindDiag(const EdgePointSet &concave_points,EdgePoint *p0, EdgePoint *&p1,EdgePoint *&p2)
{
   EdgePoint *p = p0;
   int point;
   double beta;
   do {
      if (concave_points.find(p)==concave_points.end())
      {
         UserPoint corner = p->p;
         UserPoint v1( p->prev->p - corner ); 
         UserPoint v2( p->next->p - corner );
         double denom = v1.Cross(v2);
         for(EdgePoint *other=p->next; other!= p->prev; other = other->next)
         {
            UserPoint v( other->p-corner );
            double a = v.Cross(v2);
            double b = v1.Cross(v);
            if (a>=0.0 && b>=0.0)
            {
               // Found candidate, check for intersections ...
               EdgePoint *l=p->prev;
               for( ;l!=other->next;l=l->prev)
                  if (Intersect(v,l->p-corner,l->prev->p-corner))
                     break;
               if (l!=other->next) continue;

               EdgePoint *r=p->next;
               for(;l!=other->prev;r=r->next)
                  if (Intersect(v,r->p-corner,r->next->p-corner))
                     break;
               if (r!=other->prev) continue;

               // found !
               p1 = p;
               p2 = other;
               return true;
               }
            }
         }
         p = p->next;
      } while(p!=p0);

   return(false);
}



void ConvertOutlineToTriangles(Vertices &ioOutline,bool inAllowReverse)
{
   Vertices tri_points;
   int size = ioOutline.size();
   tri_points.reserve((size-2)*3);
   double area = 0.0;
   if (inAllowReverse)
      for(int i=2;i<size;i++)
      {
         UserPoint v_prev = ioOutline[i-1]-ioOutline[0];
         UserPoint v_next = ioOutline[i]-ioOutline[0];
         area += v_prev.Cross(v_next);
      }

   bool reverse = area < 0;
   QuickVec<EdgePoint> edges(size);
   for(int i=0;i<size;i++)
   {
      edges[i].p = ioOutline[i];
      if (reverse)
      {
         edges[i].next = &edges[ (i+size-1) % size ];
         edges[i].prev = &edges[ (i+1) % size ];
      }
      else
      {
         edges[i].next = &edges[ (i+1) % size ];
         edges[i].prev = &edges[ (i+size-1) % size ];
      }
   }

   EdgePoint *p0 = &edges[0];
   EdgePoint *pi=0;

   bool force_ear = false;
   while(size>2)
   {
      EdgePointSet concave_points;
      EdgePoint *p_con = p0;

      do
      {
         if (p_con->concave())
            concave_points.insert(p_con);
         p_con = p_con->next;
      } while(p_con!=p0);

      if (!force_ear)
         pi = p0->next->next;

      while((force_ear || pi!=p0) && size>2)
      {
         if (concave_points.empty() || IsEar(concave_points,pi) || force_ear)
         {
            // Have ear triangle - yay - clip it
            if (!force_ear || pi->prev->convex())
            {
               tri_points.push_back(pi->prev->prev->p);
               tri_points.push_back(pi->prev->p);
               tri_points.push_back(pi->p);
            }

            pi->prev->prev->next = pi;
            pi->prev = pi->prev->prev;
            // Have we become concave or convex ?
            Recalc(concave_points,pi);
            // Has the previous one become convex ?
            Recalc(concave_points,pi->prev);

            // Start at p0 again ...
            if (pi->prev == p0)
               pi=pi->next;

            size --;
            force_ear = false;
         }
         else
            pi = pi->next;
      }
      if (size>2 )
      {
         EdgePoint *b1=0,*b2=0;
         if ( FindDiag(concave_points,p0,b1,b2))
         {
            // Call recursively...
            Vertices loop1;
            loop1.reserve(size);
            EdgePoint *p;
            for(p=b1;p!=b2;p=p->next)
               loop1.push_back(p->p);
            loop1.push_back(p->p);

            ConvertOutlineToTriangles(loop1,false);
            ioOutline.append(loop1);


            Vertices loop2;
            loop2.reserve(size);
            for(p=b2;p!=b1;p=p->next)
               loop2.push_back(p->p);
            loop2.push_back(p->p);
            ConvertOutlineToTriangles(loop2,false);
            ioOutline.append(loop2);
            break;
         }
         else
         {
            #if 1
            break;
            #else
            // Hmmm look for "least concave" point ...
            pi = p0->next->next;
            double best_val = -1e99;
            EdgePoint *least_concave = 0;
            double smallest_val = 1e99;
            EdgePoint *smallest = 0;
            while(pi!=p0)
            {
               if (concave_points.find(pi->prev)!=concave_points.end())
               {
                  double cross = pi->Cross();
                  if (cross>best_val)
                  {
                     best_val = cross;
                     least_concave = pi;
                  }
               }
               else if (!least_concave)
               {
                  double cross = pi->Cross();
                  if (cross<smallest_val)
                  {
                     smallest_val = cross;
                     smallest = pi;
                  }
               }
               pi = pi->next;
            }

            if (least_concave)
               pi = least_concave;
            else
               pi = smallest;

            force_ear = true;
            #endif
         }
      }
   }
   tri_points.swap(ioOutline);
}

// --- External interface ----------

void ConvertOutlineToTriangles(Vertices &ioOutline)
{
   ConvertOutlineToTriangles(ioOutline,true);
}

} // end namespace nme