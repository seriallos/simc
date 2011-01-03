// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.h"

// ==========================================================================
// BUFF
// ==========================================================================

// buff_t::buff_t ===========================================================

buff_t::buff_t( sim_t*             s,
                const std::string& n,
                int                ms,
                double             d,
                double             cd,
                double             ch,
                bool               q,
                bool               r,
                int                rt,
                int                id ) :
  spell_id_t( 0, n.c_str() ),
  sim( s ), player( 0 ), source( 0 ), name_str( n ), 
  max_stack( ms ), buff_duration( d ), buff_cooldown( cd ), default_chance( ch ),
  reverse( r ), constant( false ), quiet( q ), aura_id( id ), rng_type( rt )
{
  init();
}

// buff_t::buff_t ===========================================================

buff_t::buff_t( player_t*          p,
                const std::string& n,
                int                ms,
                double             d,
                double             cd,
                double             ch,
                bool               q,
                bool               r,
                int                rt,
                int                id ) :
  spell_id_t( p, n.c_str() ),
  sim( p -> sim ), player( p ), source( p ), name_str( n ), 
  max_stack( ms ), buff_duration( d ), buff_cooldown( cd ), default_chance( ch ),
  reverse( r ), constant( false), quiet( q ), aura_id( id ), rng_type( rt )
{
  init();
}

// buff_t::buff_t ===========================================================

buff_t::buff_t( player_t* p,
                talent_t* talent, ... ) :
  spell_id_t( p, talent -> trigger ? talent -> trigger -> name : talent -> td -> name ),
  sim( p -> sim ), player( p ), source( p ), name_str( s_token ), 
  max_stack( 0 ), buff_duration( 0 ), buff_cooldown( 0 ), default_chance( 0 ),
  reverse( false ), constant( false ), quiet( false ), rng_type( RNG_CYCLIC )
{
  if( talent -> rank() )
  {
    default_chance = talent -> sd -> proc_chance / 100.0;
    if( talent -> chance ) default_chance = talent -> chance;
    if( default_chance <= 0 ) default_chance = 1.0;

    spell_data_t* spell = talent -> trigger ? talent -> trigger : talent -> sd;

    max_stack = std::max( (int) spell -> max_stack, 1 );
    buff_duration = spell -> duration / 1000.0;
    buff_cooldown = spell -> cooldown / 1000.0;
    aura_id = spell -> id;
  }

  va_list vap;
  va_start( vap, talent );
  parse_options( vap );

  init();
}

// buff_t::buff_t ===========================================================

buff_t::buff_t( player_t*     p,
                spell_data_t* spell, ... ) :
  spell_id_t( p, spell -> name, spell -> id ),
  sim( p -> sim ), player( p ), source( p ), name_str( s_token ), 
  max_stack( 0 ), buff_duration( 0 ), buff_cooldown( 0 ), default_chance( 0 ),
  reverse( false ), constant( false ), quiet( false ), rng_type( RNG_CYCLIC )
{
  max_stack = std::max( (int) spell -> max_stack, 1 );
  default_chance = spell -> proc_chance ? ( spell -> proc_chance / 100.0 ) : 1.0;
  buff_duration = spell -> duration / 1000.0;
  buff_cooldown = spell -> cooldown / 1000.0;
  aura_id = spell -> id;

  va_list vap;
  va_start( vap, spell );
  parse_options( vap );

  init();
}

// buff_t::parse_options ====================================================

void buff_t::parse_options( va_list vap )
{
  while ( true )
  {
    const char* parm = ( const char * ) va_arg( vap, const char * );
    if( ! parm ) break;

    if( ! strcmp( parm, "stacks" ) )
    {
      max_stack = (int) va_arg( vap, int );
    }
    else if( ! strcmp( parm, "chance" ) )
    {
      default_chance = (double) va_arg( vap, double );
    }
    else if( ! strcmp( parm, "duration" ) )
    {
      buff_duration = (double) va_arg( vap, double );
    }
    else if( ! strcmp( parm, "cooldown" ) )
    {
      buff_cooldown = (double) va_arg( vap, double );
    }
    else if( ! strcmp( parm, "reverse" ) )
    {
      reverse = va_arg( vap, int ) ? true : false;
    }
    else if( ! strcmp( parm, "quiet" ) )
    {
      quiet = va_arg( vap, int ) ? true : false;
    }
    else if( ! strcmp( parm, "rng" ) )
    {
      rng_type = (int) va_arg( vap, int );
    }
  }
  va_end( vap );
}

// buff_t::init =============================================================

void buff_t::init()
{
  current_stack = 0;
  current_value = 0;
  last_start = -1;
  last_trigger = -1;
  start_intervals_sum = 0;
  trigger_intervals_sum = 0;
  uptime_sum = 0;
  up_count = 0;
  down_count = 0;
  start_intervals = 0;
  trigger_intervals = 0;
  start_count = 0;
  refresh_count = 0;
  trigger_attempts = 0;
  trigger_successes = 0;
  uptime_pct = 0;
  benefit_pct = 0;
  trigger_pct = 0;
  avg_start_interval = 0;
  avg_trigger_interval = 0;
  avg_start = 0;
  avg_refresh = 0;
  constant = false;
  expiration = 0;

  buff_t** tail=0;

  if( player )
  {
    cooldown = player -> get_cooldown( "buff_" + name_str );
    rng = player -> get_rng( name_str, rng_type );
    tail = &( player -> buff_list );
  }
  else
  {
    cooldown = sim -> get_cooldown( "buff_" + name_str );
    rng = sim -> get_rng( name_str, rng_type );
    tail = &( sim -> buff_list );
  }

  cooldown -> duration = std::min( buff_cooldown, sim -> wheel_seconds - 2.0 );

  while ( *tail && name_str > ( ( *tail ) -> name_str ) )
  {
    tail = &( ( *tail ) -> next );
  }
  next = *tail;
  *tail = this;

  if( max_stack >= 0 )
  {
    stack_occurrence.resize( max_stack + 1 );
    aura_str.resize( max_stack + 1 );

    char *buffer = new char[ name_str.size() + 16 ];

    for ( int i=1; i <= max_stack; i++ )
    {
      sprintf( buffer, "%s(%d)", name_str.c_str(), i );
      aura_str[ i ] = buffer;
    }
    delete [] buffer;
  }
}

// buff_t::buff_t ===========================================================

buff_t::buff_t( player_t*          p,
                const std::string& n,
                const char*        sname,
                double             chance,
                double             cd,
                bool               q,
                bool               r,
                int                rt ) :
  spell_id_t( p, n.c_str(), sname, 0 ),
  sim( p -> sim ), player( p ), name_str( n ),
  max_stack( ( max_stacks()!=0 ) ? max_stacks() : (initial_stacks() != 0 ? initial_stacks() : 1 ) ),
  buff_duration( ( duration() > ( p -> sim -> wheel_seconds - 2.0 ) ) ?  ( p -> sim -> wheel_seconds - 2.0 ) : duration() ),
  default_chance( (chance != -1) ? chance : ( ( proc_chance() != 0 ) ? proc_chance() : 1.0 ) ) ,
  reverse( r ), constant( false), quiet( q ), aura_id( 0 ), rng_type( rt )
{
  _init_buff_t();

  cooldown = player -> get_cooldown( "buff_" + name_str );
  if ( cd < 0.0 )
  {
    cooldown -> duration = player -> player_data.spell_cooldown( spell_id() );
  }
  else
  {
    cooldown -> duration = cd;
  }

  if ( cooldown -> duration > ( sim -> wheel_seconds - 2.0 ) )
    cooldown -> duration = sim -> wheel_seconds - 2.0;

  rng = player -> get_rng( n, rng_type );
  buff_t** tail = &(  player -> buff_list );

  while ( *tail && name_str > ( ( *tail ) -> name_str ) )
  {
    tail = &( ( *tail ) -> next );
  }
  next = *tail;
  *tail = this;
  
  if ( sim -> debug )
    log_t::output( sim, "Buff Spell status: %s", to_str().c_str() );
}

// buff_t::buff_t ===========================================================

buff_t::buff_t( player_t*          p,
                const uint32_t     id,
                const std::string& n,
                double             chance,
                double             cd,
                bool               q,
                bool               r,
                int                rt ) :
  spell_id_t( p, n.c_str(), id ),
  sim( p -> sim ), player( p ), name_str( n ),
  max_stack( ( max_stacks()!=0 ) ? max_stacks() : (initial_stacks() != 0 ? initial_stacks() : 1 ) ),
  buff_duration( ( duration() > ( p -> sim -> wheel_seconds - 2.0 ) ) ?  ( p -> sim -> wheel_seconds - 2.0 ) : duration() ),
  default_chance( (chance != -1) ? chance : ( ( proc_chance() != 0 ) ? proc_chance() : 1.0 ) ) ,
  reverse( r ), constant( false), quiet( q ), aura_id( 0 ), rng_type( rt )
{
  _init_buff_t();

  cooldown = player -> get_cooldown( "buff_" + name_str );
  if ( cd < 0.0 )
  {
    cooldown -> duration = player -> player_data.spell_cooldown( spell_id() );
  }
  else
  {
    cooldown -> duration = cd;
  }

  if ( cooldown -> duration > ( sim -> wheel_seconds - 2.0 ) )
    cooldown -> duration = sim -> wheel_seconds - 2.0;

  rng = player -> get_rng( n, rng_type );
  buff_t** tail = &(  player -> buff_list );

  while ( *tail && name_str > ( ( *tail ) -> name_str ) )
  {
    tail = &( ( *tail ) -> next );
  }
  next = *tail;
  *tail = this;
  
  if ( sim -> debug )
    log_t::output( sim, "Buff Spell status: %s", to_str().c_str() );
}

// buff_t::_init_buff_t ====================================================

void buff_t::_init_buff_t()
{
  // FIXME! For the love of all that is holy.... FIXME!
  // This routine will disappear once data-access rework is complete

  current_stack = 0;
  current_value = 0;
  last_start = -1;
  last_trigger = -1;
  start_intervals_sum = 0;
  trigger_intervals_sum = 0;
  uptime_sum = 0;
  up_count = 0;
  down_count = 0;
  start_intervals = 0;
  trigger_intervals = 0;
  start_count = 0;
  refresh_count = 0;
  trigger_attempts = 0;
  trigger_successes = 0;
  uptime_pct = 0;
  benefit_pct = 0;
  trigger_pct = 0;
  avg_start_interval = 0;
  avg_trigger_interval = 0;
  avg_start = 0;
  avg_refresh = 0;
  constant = false;
  expiration = 0;

  if( max_stack >= 0 )
  {
    stack_occurrence.resize( max_stack + 1 );
    aura_str.resize( max_stack + 1 );

    char *buffer = new char[ name_str.size() + 16 ];

    for ( int i=1; i <= max_stack; i++ )
    {
      sprintf( buffer, "%s(%d)", name_str.c_str(), i );
      aura_str[ i ] = buffer;
    }
    delete [] buffer;
  }
}

// buff_t::may_react ========================================================

bool buff_t::may_react( int stack )
{
  if ( current_stack == 0    ) return false;
  if ( stack > current_stack ) return false;
  if ( stack < 1             ) return false;

  if( stack > max_stack ) return false;

  double occur = stack_occurrence[ stack ];

  if ( occur <= 0 ) return true;

  return sim -> time_to_think( occur );
}

// buff_t::stack_react ======================================================

int buff_t::stack_react()
{
  int stack = 0;

  for( int i=1; i <= current_stack; i++ )
  {
    if ( ! sim -> time_to_think( stack_occurrence[ i ] ) ) break;
    stack++;
  }

  return stack;
}

// buff_t::remains ==========================================================

double buff_t::remains()
{
  if ( current_stack <= 0 )
  {
    return 0;
  }
  if ( expiration )
  {
    return expiration -> occurs() - sim -> current_time;
  }
  return -1;
}

// buff_t::remains_gt =======================================================

bool buff_t::remains_gt( double time )
{
  double time_remaining = remains();

  if ( time_remaining == 0 ) return false;

  if ( time_remaining == -1 ) return true;

  return ( time_remaining > time );
}

// buff_t::remains_lt =======================================================

bool buff_t::remains_lt( double time )
{
  double time_remaining = remains();

  if ( time_remaining == -1 ) return false;

  return ( time_remaining < time );
}

// buff_t::trigger ==========================================================

bool buff_t::trigger( action_t* a,
                      int       stacks,
                      double    value )
{
  double chance = default_chance;
  if ( chance < 0 ) chance = a -> ppm_proc_chance( -chance );
  return trigger( stacks, value, chance );
}


// buff_t::trigger ==========================================================

bool buff_t::trigger( int    stacks,
                      double value,
                      double chance )
{
  if ( max_stack == 0 || chance == 0 ) return false;

  if ( cooldown -> remains() > 0 )
    return false;

  trigger_attempts++;

  if ( chance < 0 ) chance = default_chance;

  if ( ! rng -> roll( chance ) )
    return false;

  if( last_trigger > 0 ) 
  {
    trigger_intervals_sum += sim -> current_time - last_trigger;
    trigger_intervals++;
  }
  last_trigger = sim -> current_time;

  if ( reverse )
  {
    decrement( stacks, value );
  }
  else
  {
    increment( stacks, value );
  }

  // new buff cooldown impl
  if ( cooldown -> duration > 0 )
  {
    if ( sim -> debug ) log_t::output( sim, "%s starts cooldown for %s (%s)", player -> name(), name(), cooldown -> name() );

    cooldown -> start();
  }

  trigger_successes++;

  return true;
}

// buff_t::increment ========================================================

void buff_t::increment( int    stacks,
                        double value )
{
  if ( max_stack == 0 ) return;

  if ( current_stack == 0 )
  {
    start( stacks, value );
  }
  else
  {
    refresh( stacks, value );
  }
}

// buff_t::decrement ========================================================

void buff_t::decrement( int    stacks,
                        double value )
{
  if ( max_stack == 0 || current_stack <= 0 ) return;

  if ( stacks == 0 || current_stack <= stacks )
  {
    expire();
  }
  else
  {
    current_stack -= stacks;
    if ( value >= 0 ) current_value = value;
  }
}

// buff_t::start ============================================================

void buff_t::start( int    stacks,
                    double value )
{
  if ( max_stack == 0 ) return;

  assert( current_stack == 0 );

  if ( sim -> current_time <= 0.01 ) constant = true;

  start_count++;

  bump( stacks, value );

  if ( last_start >= 0 )
  {
    start_intervals_sum += sim -> current_time - last_start;
    start_intervals++;
  }
  last_start = sim -> current_time;

  if ( buff_duration > 0 )
  {
    struct expiration_t : public event_t
    {
      buff_t* buff;
      expiration_t( sim_t* sim, player_t* p, buff_t* b ) : event_t( sim, p ), buff( b )
      {
        name = buff -> name();
        sim -> add_event( this, buff -> buff_duration );
      }
      virtual void execute()
      {
        buff -> expiration = 0;
        buff -> expire();
      }
    };

    expiration = new ( sim ) expiration_t( sim, player, this );
  }
}

// buff_t::refresh ==========================================================

void buff_t::refresh( int    stacks,
                      double value )
{
  if ( max_stack == 0 ) return;

  refresh_count++;

  bump( stacks, value );

  if ( buff_duration > 0 )
  {
    assert( expiration );
    if( expiration -> occurs() < sim -> current_time + buff_duration )
    {
      expiration -> reschedule( buff_duration );
    }
  }
}

// buff_t::bump =============================================================

void buff_t::bump( int    stacks,
                   double value )
{
  if ( max_stack == 0 ) return;

  if ( max_stack < 0 )
  {
    current_stack += stacks;
  }
  else if ( current_stack < max_stack )
  {
    int before_stack = current_stack;

    current_stack += stacks;
    if ( current_stack > max_stack )
      current_stack = max_stack;

    aura_gain();

    for( int i=before_stack+1; i <= current_stack; i++ )
    {
      stack_occurrence[ i ] = sim -> current_time;
    }
  }

  if ( value >= 0 ) current_value = value;
}

// buff_t::override =========================================================

void buff_t::override( int    stacks,
                       double value )
{
  if( max_stack == 0 ) return;
  if ( current_stack != 0 )
  {
    sim -> errorf( "buff_t::override assertion error current_stack is not zero, buff %s.\n", name() );
    assert( 0 );
  }
  //assert( current_stack == 0 );
  buff_duration = 0;
  start( stacks, value );
}

// buff_t::expire ===========================================================

void buff_t::expire()
{
  if ( current_stack <= 0 ) return;
  event_t::cancel( expiration );
  source = 0;
  current_stack = 0;
  current_value = 0;
  aura_loss();
  if ( last_start >= 0 )
  {
    double current_time = player ? ( player -> current_time ) : ( sim -> current_time );
    uptime_sum += current_time - last_start;
  }
  if ( sim -> target -> initial_health == 0 ||
       sim -> target -> current_health > 0 ) 
  {
    constant = false;
  }
}

// buff_t::predict ==========================================================

void buff_t::predict()
{
  // Guarantee that may_react() will return true if the buff is present.

  for( int i=0; i <= current_stack; i++ ) 
  {
    stack_occurrence[ i ] = -1;
  }
}

// buff_t::aura_gain ========================================================

void buff_t::aura_gain()
{
  if ( sim -> log )
  {
    const char* s = ( max_stack < 0 ) ? name() : aura_str[ current_stack ].c_str();

    if ( player )
    {
      player -> aura_gain( s, aura_id );
    }
    else
    {
      sim -> aura_gain( s, aura_id );
    }
  }
}

// buff_t::aura_loss ========================================================

void buff_t::aura_loss()
{
  if ( player )
  {
    player -> aura_loss( name(), aura_id );
  }
  else
  {
    sim -> aura_loss( name(), aura_id );
  }
}

// buff_t::reset ============================================================

void buff_t::reset()
{
  cooldown -> reset();
  expire();
  last_start = -1;
  last_trigger = -1;
}

// buff_t::merge ============================================================

void buff_t::merge( buff_t* other )
{
  start_intervals_sum   += other -> start_intervals_sum;
  trigger_intervals_sum += other -> trigger_intervals_sum;
  uptime_sum            += other -> uptime_sum;
  up_count              += other -> up_count;
  down_count            += other -> down_count;
  start_intervals       += other -> start_intervals;
  trigger_intervals     += other -> trigger_intervals;
  start_count           += other -> start_count;
  refresh_count         += other -> refresh_count;
  trigger_attempts      += other -> trigger_attempts;
  trigger_successes     += other -> trigger_successes;
}

// buff_t::analyze ==========================================================

void buff_t::analyze()
{
  double total_seconds = player ? player -> total_seconds : sim -> total_seconds;
  if ( total_seconds > 0 )
  {
    uptime_pct = 100.0 * uptime_sum / total_seconds;
  }
  if ( up_count > 0 )
  {
    benefit_pct = 100.0 * up_count / ( up_count + down_count );
  }
  if ( trigger_attempts > 0 )
  {
    trigger_pct = 100.0 * trigger_successes / trigger_attempts;
  }
  if ( start_intervals > 0 )
  {
    avg_start_interval = start_intervals_sum / start_intervals;
  }
  if ( trigger_intervals > 0 )
  {
    avg_trigger_interval = trigger_intervals_sum / trigger_intervals;
  }
  avg_start   =   start_count / ( double ) sim -> iterations;
  avg_refresh = refresh_count / ( double ) sim -> iterations;
}

// buff_t::find =============================================================

buff_t* buff_t::find( sim_t* sim,
                      const std::string& name_str )
{
  for ( buff_t* b = sim -> buff_list; b; b = b -> next )
    if ( name_str == b -> name() )
      return b;

  return 0;
}

// buff_t::find =============================================================

buff_t* buff_t::find( player_t* p,
                      const std::string& name_str )
{
  for ( buff_t* b = p -> buff_list; b; b = b -> next )
    if ( name_str == b -> name() )
      return b;

  return 0;
}

std::string buff_t::to_str() SC_CONST
{
  std::ostringstream s;
  
  s << spell_id_t::to_str();
  s << " max_stack=" << max_stack;
  s << " initial_stack=" << initial_stacks();
  s << " cooldown=" << cooldown -> duration;
  s << " duration=" << buff_duration;
  s << " default_chance=" << default_chance;
  
  return s.str();
}

// buff_t::create_expression ================================================

action_expr_t* buff_t::create_expression( action_t* action,
                                          const std::string& type )
{
  if ( type == "remains" )
  {
    struct buff_remains_expr_t : public action_expr_t
    {
      buff_t* buff;
      buff_remains_expr_t( action_t* a, buff_t* b ) : action_expr_t( a, "buff_remains", TOK_NUM ), buff(b) {}
      virtual int evaluate() { result_num = buff -> remains(); return TOK_NUM; }
    };
    return new buff_remains_expr_t( action, this );
  }
  else if ( type == "cooldown_remains" )
  {
    struct buff_cooldown_remains_expr_t : public action_expr_t
    {
      buff_t* buff;
      buff_cooldown_remains_expr_t( action_t* a, buff_t* b ) : action_expr_t( a, "buff_cooldown_remains", TOK_NUM ), buff(b) {}
      virtual int evaluate() { result_num = buff -> cooldown -> remains(); return TOK_NUM; }
    };
    return new buff_cooldown_remains_expr_t( action, this );
  }
  else if ( type == "up" )
  {
    struct buff_up_expr_t : public action_expr_t
    {
      buff_t* buff;
      buff_up_expr_t( action_t* a, buff_t* b ) : action_expr_t( a, "buff_up", TOK_NUM ), buff(b) {}
      virtual int evaluate() { result_num = ( buff -> check() > 0 ) ? 1.0 : 0.0; return TOK_NUM; }
    };
    return new buff_up_expr_t( action, this );
  }
  else if ( type == "down" )
  {
    struct buff_down_expr_t : public action_expr_t
    {
      buff_t* buff;
      buff_down_expr_t( action_t* a, buff_t* b ) : action_expr_t( a, "buff_down", TOK_NUM ), buff(b) {}
      virtual int evaluate() { result_num = ( buff -> check() <= 0 ) ? 1.0 : 0.0; return TOK_NUM; }
    };
    return new buff_down_expr_t( action, this );
  }
  else if ( type == "stack" )
  {
    struct buff_stack_expr_t : public action_expr_t
    {
      buff_t* buff;
      buff_stack_expr_t( action_t* a, buff_t* b ) : action_expr_t( a, "buff_stack", TOK_NUM ), buff(b) {}
      virtual int evaluate() { result_num = buff -> check(); return TOK_NUM; }
    };
    return new buff_stack_expr_t( action, this );
  }
  else if ( type == "react" )
  {
    struct buff_react_expr_t : public action_expr_t
    {
      buff_t* buff;
      buff_react_expr_t( action_t* a, buff_t* b ) : action_expr_t( a, "buff_react", TOK_NUM ), buff(b) {}
      virtual int evaluate() { result_num = buff -> stack_react(); return TOK_NUM; }
    };
    return new buff_react_expr_t( action, this );
  }
  else if ( type == "cooldown_react" )
  {
    struct buff_cooldown_react_expr_t : public action_expr_t
    {
      buff_t* buff;
      buff_cooldown_react_expr_t( action_t* a, buff_t* b ) : action_expr_t( a, "buff_cooldown_react", TOK_NUM ), buff(b) {}
      virtual int evaluate() 
      { 
        if ( buff -> check() && ! buff -> may_react() )
        {
          result_num = 0;
        }
        else
        {
          result_num = buff -> cooldown -> remains();
        }
        return TOK_NUM; 
      }
    };
    return new buff_cooldown_react_expr_t( action, this );
  }

  return 0;
}

// ==========================================================================
// STAT_BUFF
// ==========================================================================

// stat_buff_t::stat_buff_t =================================================

stat_buff_t::stat_buff_t( player_t*          p,
                          const std::string& n,
                          int                st,
                          double             a,
                          int                ms,
                          double             d,
                          double             cd,
                          double             ch,
                          bool               q,
                          bool               r,
                          int                rng_type,
                          int                id ) :
  buff_t( p, n, ms, d, cd, ch, q, r, rng_type, id ), stat(st), amount(a)
{
}

// stat_buff_t::stat_buff_t =================================================

stat_buff_t::stat_buff_t( player_t*          p,
                          const uint32_t     id,
                          const std::string& n,
                          int                st,
                          double             a,
                          double             ch,
                          double             cd,
                          bool               q,
                          bool               r,
                          int                rng_type ) :
  buff_t( p, id, n, ch, cd, q, r, rng_type ), stat(st), amount(a)
{
}

// stat_buff_t::bump ========================================================

void stat_buff_t::bump( int    stacks,
                        double value )
{
  if ( max_stack == 0 ) return;
  if ( value > 0 ) 
  {
    if ( value < amount ) return;
    amount = value;
  }
  buff_t::bump( stacks );
  double delta = amount * current_stack - current_value;
  if( delta > 0 )
  {
    player -> stat_gain( stat, delta );
    current_value += delta;
  }
  else assert( delta == 0 );
}

// stat_buff_t::decrement ===================================================

void stat_buff_t::decrement( int    stacks,
                             double value )
{
  if ( max_stack == 0 ) return;
  if ( stacks == 0 || current_stack <= stacks )
  {
    expire();
  }
  else
  {
    double delta = amount * stacks;
    player -> stat_loss( stat, delta );
    current_stack -= stacks;
    current_value -= delta;
  }
}

// stat_buff_t::expire ======================================================

void stat_buff_t::expire()
{
  if ( current_stack > 0 )
  {
    player -> stat_loss( stat, current_value );
    buff_t::expire();
  }
}

// ==========================================================================
// DEBUFF
// ==========================================================================

// debuff_t::debuff_t =======================================================

debuff_t::debuff_t( target_t*          t,
                    const std::string& n,
                    int                ms,
                    double             d,
                    double             cd,
                    double             ch,
                    bool               q,
                    bool               r,
                    int                rng_type,
                    int                id ) :
  buff_t( t -> sim, n, ms, d, cd, ch, q, r, rng_type, id ), target( t )

{
}

// debuff_t::aura_gain ======================================================

void debuff_t::aura_gain()
{
  if ( sim -> log )
  {
    const char* s = ( max_stack < 0 ) ? name() : aura_str[ current_stack ].c_str();

    target -> aura_gain( s, aura_id );
  }
}

// debuff_t::aura_loss ======================================================

void debuff_t::aura_loss()
{
  target -> aura_loss( name(), aura_id );
}


// ==========================================================================
// Generic Passive Buff system
// ==========================================================================

new_buff_t::new_buff_t( player_t*          p, 
                        const std::string& n,
                        uint32_t           id,
                        double             override_chance,
                        bool               quiet,
                        bool               reverse,
                        int                rt ) :
  buff_t( p, n, 1, 0, 0, override_chance, quiet, reverse, rt, id ),
  default_stack_charge( 1 ), single( 0 )
{
  uint32_t effect_id;
  int        effects = 0;
  
  memset(e_data, 0, sizeof(e_data));
  
  // Find some stuff for the buff, generically
  if ( id > 0 && player -> player_data.spell_exists( id ) )
  {
    max_stack            = player -> player_data.spell_max_stacks( id );

    buff_duration             = player -> player_data.spell_duration( id );
    if ( buff_duration > ( player -> sim -> wheel_seconds - 2.0 ) )
      buff_duration = player -> sim -> wheel_seconds - 2.0;

    cooldown -> duration     = player -> player_data.spell_cooldown( id );
    if ( cooldown -> duration > ( player -> sim -> wheel_seconds - 2.0 ) )
      cooldown -> duration = player -> sim -> wheel_seconds - 2.0;

    default_stack_charge = player -> player_data.spell_initial_stacks( id );

    if ( override_chance == 0 )
    {
      default_chance     = player -> player_data.spell_proc_chance( id );
      
      if ( default_chance == 0.0 )
        default_chance   = 1.0;
    }

    // If there's no max stack, set max stack to proc charges
    if ( max_stack == 0 )
      max_stack          = player -> player_data.spell_initial_stacks( id );

    // A duration of < 0 in blizzard speak is infinite -> change to 0
    if ( buff_duration < 0 )
      buff_duration           = 0.0;

    // Some spells seem to have a max_stack but no initial stacks, so set 
    // initial stacks to 1 if it is 0 in spell data
    if ( default_stack_charge == 0 ) 
      default_stack_charge = 1;

    // Some spells do not have a max stack, but we need to at least have 
    // a single application for a buff. Set the max stack (if 0) to the 
    // initial stack value
    if ( max_stack == 0 )
      max_stack            = default_stack_charge;

    // Map effect data for this buff
    for ( int i = 1; i <= MAX_EFFECTS; i++ ) 
    {
      if ( ! ( effect_id = player -> player_data.spell_effect_id( id, i ) ) )
        continue;
      
      e_data[ i - 1 ] = player -> player_data.m_effects_index[ effect_id ];

      // Trigger spells will not be used in anything for now
      if ( e_data[ i - 1 ] -> trigger_spell_id > 0 )
        continue;

      effects++;
    }
    
    // Optimize if there's only a single effect to use
    if ( effects < 2 )
    {
      for ( int i = 0; i < MAX_EFFECTS; i++ ) 
      {
        if ( ! e_data[ i ] || e_data[ i ] -> trigger_spell_id > 0 )
          continue;
        
        single = e_data[ i ];
        break;
      }
    }
    
    if ( sim -> debug )
    {
      log_t::output( sim, "Initializing New Buff %s id=%d max_stack=%d default_charges=%d cooldown=%.2f duration=%.2f default_chance=%.2f atomic=%s",
        name_str.c_str(), id, max_stack, default_stack_charge, cooldown -> duration, buff_duration, default_chance,
        single ? "true" : "false");
    }
  }
  
  _init_buff_t();
}

bool new_buff_t::trigger( int stacks, double value, double chance)
{
  // For buffs going up, use the default_stack_charge as the amount
  return buff_t::trigger( stacks == -1 ? default_stack_charge : stacks, value, chance );
}

double new_buff_t::base_value( effect_type_t type, effect_subtype_t sub_type, int misc_value, int misc_value2 ) SC_CONST
{
  if ( single )
    return sc_data_access_t::fmt_value( single -> base_value, (effect_type_t) single -> type, (effect_subtype_t) single -> subtype );

  for ( int i = 0; i < MAX_EFFECTS; i++ )
  {
    if ( ! e_data[ i ] )
      continue;

    if ( ( type == E_MAX || e_data[ i ] -> type == type ) && 
         ( sub_type == A_MAX || e_data[ i ] -> subtype == sub_type ) && 
         ( misc_value == DEFAULT_MISC_VALUE || e_data[ i ] -> misc_value == misc_value ) &&
         ( misc_value2 == DEFAULT_MISC_VALUE || e_data[ i ] -> misc_value_2 == misc_value2 ) )
      return sc_data_access_t::fmt_value( e_data[ i ] -> base_value, type, sub_type );
  }
  
  return 0.0;
}

