      module omp_lib_kinds
        implicit none
        integer, parameter :: omp_integer_kind = 4
        integer, parameter :: omp_logical_kind = 4
        integer, parameter :: omp_lock_kind = 8
        integer, parameter :: omp_nest_lock_kind = 8
        integer, parameter :: omp_sched_kind = 4
      end module

      module omp_lib
        use omp_lib_kinds
        implicit none
        integer, parameter :: openmp_version = 200805
        integer (omp_sched_kind), parameter :: omp_sched_static = 1
        integer (omp_sched_kind), parameter :: omp_sched_dynamic = 2
        integer (omp_sched_kind), parameter :: omp_sched_guided = 3
        integer (omp_sched_kind), parameter :: omp_sched_auto = 4

        interface
          subroutine omp_init_lock (lock)
            use omp_lib_kinds
            integer (omp_lock_kind), intent (out) :: lock
          end subroutine omp_init_lock
        end interface

        interface
          subroutine omp_init_nest_lock (lock)
            use omp_lib_kinds
            integer (omp_nest_lock_kind), intent (out) :: lock
          end subroutine omp_init_nest_lock
        end interface

        interface
          subroutine omp_destroy_lock (lock)
            use omp_lib_kinds
            integer (omp_lock_kind), intent (inout) :: lock
          end subroutine omp_destroy_lock
        end interface

        interface
          subroutine omp_destroy_nest_lock (lock)
            use omp_lib_kinds
            integer (omp_nest_lock_kind), intent (inout) :: lock
          end subroutine omp_destroy_nest_lock
        end interface

        interface
          subroutine omp_set_lock (lock)
            use omp_lib_kinds
            integer (omp_lock_kind), intent (inout) :: lock
          end subroutine omp_set_lock
        end interface

        interface
          subroutine omp_set_nest_lock (lock)
            use omp_lib_kinds
            integer (omp_nest_lock_kind), intent (inout) :: lock
          end subroutine omp_set_nest_lock
        end interface

        interface
          subroutine omp_unset_lock (lock)
            use omp_lib_kinds
            integer (omp_lock_kind), intent (inout) :: lock
          end subroutine omp_unset_lock
        end interface

        interface
          subroutine omp_unset_nest_lock (lock)
            use omp_lib_kinds
            integer (omp_nest_lock_kind), intent (inout) :: lock
          end subroutine omp_unset_nest_lock
        end interface

        interface omp_set_dynamic
          subroutine omp_set_dynamic (set)
            logical (4), intent (in) :: set
          end subroutine omp_set_dynamic
          subroutine omp_set_dynamic_8 (set)
            logical (8), intent (in) :: set
          end subroutine omp_set_dynamic_8
        end interface

        interface omp_set_nested
          subroutine omp_set_nested (set)
            logical (4), intent (in) :: set
          end subroutine omp_set_nested
          subroutine omp_set_nested_8 (set)
            logical (8), intent (in) :: set
          end subroutine omp_set_nested_8
        end interface

        interface omp_set_num_threads
          subroutine omp_set_num_threads (set)
            integer (4), intent (in) :: set
          end subroutine omp_set_num_threads
          subroutine omp_set_num_threads_8 (set)
            integer (8), intent (in) :: set
          end subroutine omp_set_num_threads_8
        end interface

        interface
          function omp_get_dynamic ()
            use omp_lib_kinds
            logical (omp_logical_kind) :: omp_get_dynamic
          end function omp_get_dynamic
        end interface

        interface
          function omp_get_nested ()
            use omp_lib_kinds
            logical (omp_logical_kind) :: omp_get_nested
          end function omp_get_nested
        end interface

        interface
          function omp_in_parallel ()
            use omp_lib_kinds
            logical (omp_logical_kind) :: omp_in_parallel
          end function omp_in_parallel
        end interface

        interface
          function omp_test_lock (lock)
            use omp_lib_kinds
            logical (omp_logical_kind) :: omp_test_lock
            integer (omp_lock_kind), intent (inout) :: lock
          end function omp_test_lock
        end interface

        interface
          function omp_get_max_threads ()
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_max_threads
          end function omp_get_max_threads
        end interface

        interface
          function omp_get_num_procs ()
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_num_procs
          end function omp_get_num_procs
        end interface

        interface
          function omp_get_num_threads ()
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_num_threads
          end function omp_get_num_threads
        end interface

        interface
          function omp_get_thread_num ()
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_thread_num
          end function omp_get_thread_num
        end interface

        interface
          function omp_test_nest_lock (lock)
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_test_nest_lock
            integer (omp_nest_lock_kind), intent (inout) :: lock
          end function omp_test_nest_lock
        end interface

        interface
          function omp_get_wtick ()
            double precision :: omp_get_wtick
          end function omp_get_wtick
        end interface

        interface
          function omp_get_wtime ()
            double precision :: omp_get_wtime
          end function omp_get_wtime
        end interface

        interface
          function omp_in_final()
            use omp_lib_kinds
            logical (omp_logical_kind) :: omp_in_final
          end function omp_in_final
        end interface

        interface omp_set_schedule
          subroutine omp_set_schedule(kind, modifier)
            use omp_lib_kinds
			integer (omp_sched_kind), intent (in), value :: kind
            integer (omp_integer_kind), intent (in), value :: modifier
          end subroutine omp_set_schedule
          subroutine omp_set_schedule_8 (kind, modifier)
            use omp_lib_kinds
            integer (omp_sched_kind), intent (in), value  :: kind
            integer (8), intent (in), value  :: modifier
          end subroutine omp_set_schedule_8
        end interface omp_set_schedule

        interface omp_get_schedule
          subroutine omp_get_schedule(kind, modifier)
            use omp_lib_kinds
			integer (omp_sched_kind), intent (out) :: kind
            integer (omp_integer_kind), intent (out) :: modifier
          end subroutine omp_get_schedule
          subroutine omp_get_schedule_8 (kind, modifier)
            use omp_lib_kinds
            integer (omp_sched_kind), intent (out) :: kind
            integer (8), intent (out) :: modifier
          end subroutine omp_get_schedule_8
        end interface omp_get_schedule

        interface
          function omp_get_thread_limit ()
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_thread_limit
          end function omp_get_thread_limit
        end interface

        interface omp_set_max_active_levels
          subroutine omp_set_max_active_levels(max_active_levels)
            use omp_lib_kinds
            integer (omp_integer_kind) :: max_active_levels
          end subroutine omp_set_max_active_levels
          subroutine omp_set_max_active_levels_8 (max_active_levels)
            integer (8), intent (in) :: max_active_levels
          end subroutine omp_set_max_active_levels_8
        end interface

        interface
          function omp_get_max_active_levels ()
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_max_active_levels
          end function omp_get_max_active_levels
        end interface

        interface
          function omp_get_level ()
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_level
          end function omp_get_level
        end interface

        interface omp_get_ancestor_thread_num
          function omp_get_ancestor_thread_num (level)
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_ancestor_thread_num
			integer (omp_integer_kind), intent (in) :: level
          end function omp_get_ancestor_thread_num
          function omp_get_ancestor_thread_num_8 (level)
            integer (8), intent (in) :: level
            integer (4) :: omp_get_ancestor_thread_num_8
          end function omp_get_ancestor_thread_num_8
        end interface

        interface omp_get_team_size
          function omp_get_team_size (level)
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_team_size
			integer (omp_integer_kind), intent (in) :: level
          end function omp_get_team_size
          function omp_get_team_size_8 (level)
            integer (8), intent (in) :: level
            integer (4) :: omp_get_team_size_8
          end function omp_get_team_size_8
        end interface

        interface
          function omp_get_active_level ()
            use omp_lib_kinds
            integer (omp_integer_kind) :: omp_get_active_level
          end function omp_get_active_level
        end interface

      end module omp_lib
      
