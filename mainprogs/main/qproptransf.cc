// $Id: qproptransf.cc,v 1.9 2004-05-07 16:04:51 edwards Exp $
/*! \file
 *  \brief Converts quark propagators in one format into another format.
 */

#include "chroma.h"

using namespace QDP;

/*
 * Input 
 */

//! Propagator type
enum SciDACPropType {
  SCIDAC_SOURCE,
  SCIDAC_PROP,
  SCIDAC_SEQPROP,
};

//! Read a SciDACPropType enum
void read(XMLReader& xml, const string& path, SciDACPropType& param)
{
  string prop_type_str;
  read(xml, path, prop_type_str);
  if (prop_type_str == "PROPAGATOR")
    param = SCIDAC_PROP;
  else if (prop_type_str == "SOURCE")
    param = SCIDAC_SOURCE;
  else if (prop_type_str == "SEQPROP")
    param = SCIDAC_SEQPROP;
  else 
  {
    QDPIO::cerr << "Unsupported propagator type" << endl;
    QDP_abort(1);
  }
}



// Parameters which must be determined from the XML input
// and written to the XML output
struct Param_t
{
  multi1d<int> nrow;		// Lattice dimension
};

struct Prop_t
{
  PropType  prop_in_type;       // propagator format
  string    prop_in_file;

  PropType  prop_out_type;      // propagator format
  string    prop_out_file;
  QDP_volfmt_t prop_out_volfmt; // volume format (SINGLEFILE or MULTIFILE)

  SciDACPropType   scidac_prop_type;   // Either "PROPAGATOR", or "SEQPROP", etc.
};

struct QpropTransf_input_t
{
  Param_t  param;
  Cfg_t    cfg;
  Prop_t   prop;
};



//! Propagator parameters
void read(XMLReader& xml, const string& path, Prop_t& input)
{
  XMLReader inputtop(xml, path);

  read(inputtop, "prop_in_type", input.prop_in_type);
  read(inputtop, "prop_in_file", input.prop_in_file);

  read(inputtop, "prop_out_type", input.prop_out_type);
  read(inputtop, "prop_out_file", input.prop_out_file);
  read(inputtop, "prop_out_volfmt", input.prop_out_volfmt);  // singlefile or multifile

  if (inputtop.count("scidac_prop_type") != 0)
    read(inputtop, "scidac_prop_type", input.scidac_prop_type);
}


//! Parameters for running code
void read(XMLReader& xml, const string& path, Param_t& param)
{
  XMLReader paramtop(xml, path);

  int version;
  read(paramtop, "version", version);

  switch (version) 
  {
  case 1:
  case 2:
    /**************************************************************************/
    break;

  default :
    /**************************************************************************/
    QDPIO::cerr << "Input parameter version " << version << " unsupported." << endl;
    QDP_abort(1);
  }


  read(paramtop, "nrow", param.nrow);
}


// Reader for input parameters
void read(XMLReader& xml, const string& path, QpropTransf_input_t& input)
{
  XMLReader inputtop(xml, path);

  // Read all the input groups
  try
  {
    // Read program parameters
    read(inputtop, "Param", input.param);

    // Read in the gauge configuration info
    read(inputtop, "Cfg", input.cfg);

    // Read in the propagator file info
    read(inputtop, "Prop", input.prop);
  }
  catch (const string& e) 
  {
    QDPIO::cerr << "Error reading qproptransf data: " << e << endl;
    throw;
  }
}


//! Many-to-many propagator transformation routine
/*! \defgroup qproptransf Tranformation routine
 *  \ingroup main
 *
 * Main program for transforming propagator formats
 */

int main(int argc, char *argv[])
{
  // Put the machine into a known state
  QDP_initialize(&argc, &argv);

  START_CODE("qproptransf");
  
  // Parameter structure for the input
  QpropTransf_input_t input;

  // Instantiate xml reader for DATA
  XMLReader xml_in("DATA");

  // Read data
  read(xml_in, "/qproptransf", input);

  // Setup QDP
  Layout::setLattSize(input.param.nrow);
  Layout::create();

  QDPIO::cout << "QPROPTRANSF" << endl;

  XMLFileWriter xml_out("qproptransf.xml");
  push(xml_out, "qproptransf");

  proginfo(xml_out);    // Print out basic program info

  write(xml_out, "input", xml_in); // save a copy of the input
  xml_out.flush();
  
  /*
   * Now read them thangs...
   */
  XMLReader prop_in_xml, prop_in_file_xml;
  LatticePropagator  prop;

  switch (input.prop.prop_in_type)
  {
  case PROP_TYPE_SZIN:
    // SZIN
    push(xml_out,"SZIN_propagator");
    write(xml_out, "prop_in_type", input.prop.prop_in_type);
    write(xml_out, "prop_in_file", input.prop.prop_in_file);

    readSzinQprop(prop_in_xml, prop, input.prop.prop_in_file);

    write(xml_out, "propagator_info", prop_in_xml);
    pop(xml_out);
    break;

  case PROP_TYPE_SCIDAC:
    // SciDAC
    push(xml_out,"SciDAC_propagator");
    write(xml_out, "input_type", input.prop.prop_in_type);
    write(xml_out, "prop_in_file", input.prop.prop_in_file);

    readQprop(prop_in_file_xml, prop_in_xml, prop, 
	      input.prop.prop_in_file, QDPIO_SERIAL);

    write(xml_out, "File_xml", prop_in_file_xml);
    write(xml_out, "Record_xml", prop_in_xml);
    pop(xml_out);
    break;

  case PROP_TYPE_KYU:
    // Kentucky
    push(xml_out,"KYU_propagator");
    write(xml_out, "prop_in_type", input.prop.prop_in_type);
    write(xml_out, "prop_in_file", input.prop.prop_in_file);

    readKYUQprop(prop, input.prop.prop_in_file);

    pop(xml_out);
    break;

  default:
    QDP_error_exit("unknown input type", input.prop.prop_in_type);
  }
    

  // Sanity check - write out the propagator (pion) correlator in the Nd-1 direction
  {
    // Initialize the slow Fourier transform phases
    SftMom phases(0, true, Nd-1);

    multi1d<Double> prop_corr = sumMulti(localNorm2(prop), 
					 phases.getSet());

    push(xml_out, "Prop_correlator");
    write(xml_out, "prop_corr", prop_corr);
    pop(xml_out);
  }

  xml_out.flush();

  /*
   * Now write them thangs...
   */ 
  switch (input.prop.prop_out_type)
  {
  case PROP_TYPE_SZIN:
  {
    // SZIN
    Real Kappa(3.14159265359);

    push(xml_out,"SZIN_propagator");
    write(xml_out, "output_type", input.prop.prop_out_type);
    write(xml_out, "prop_out_file", input.prop.prop_out_file);
    pop(xml_out);

    writeSzinQprop(prop, input.prop.prop_out_file, Kappa);
  }
  break;

  case PROP_TYPE_SCIDAC:
  {
    // SciDAC
    // SciDAC output expects to find the relevant structures in the
    // xml input.
    // Read in the configuration along with relevant information.
    multi1d<LatticeColorMatrix> u(Nd);
    XMLReader gauge_file_xml, gauge_xml;

    // Startup gauge
    gaugeStartup(gauge_file_xml, gauge_xml, u, input.cfg);

    // xml input file
    XMLReader inputtop(xml_in, "/qproptransf");

    // There are various forms of SciDAC prop types
    switch (input.prop.scidac_prop_type)
    {
    case SCIDAC_SOURCE:
    {
      // Try to invert this record XML into a source struct
      // Also pull out the id of this source
      PropSource_t source_header;

      try
      {
	read(inputtop, "MakeSource/PropSource", source_header);
      }
      catch (const string& e) 
      {
	QDPIO::cerr << "Error extracting source_header: " << e << endl;
	throw;
      }

      {
	XMLBufferWriter prop_out_file_xml;
	push(prop_out_file_xml, "make_source");
	int id = 0;    // NEED TO FIX THIS - SOMETHING NON-TRIVIAL NEEDED
	write(prop_out_file_xml, "id", id);
	pop(prop_out_file_xml);

	XMLBufferWriter prop_out_record_xml;
	push(prop_out_record_xml, "MakeSource");
	write(prop_out_record_xml, "PropSource", source_header);
	write(prop_out_record_xml, "Config_info", gauge_xml);
	pop(prop_out_record_xml);
    
	// Write the source
	writeQprop(prop_out_file_xml, prop_out_record_xml, prop,
		   input.prop.prop_out_file, input.prop.prop_out_volfmt, 
		   QDPIO_SERIAL);
      }
    }
    break;

    case SCIDAC_PROP:
    {
      // Try to invert this record XML into a source struct
      // Also pull out the id of this source
      ChromaProp_t prop_header;
      PropSource_t source_header;

      try
      {
	read(inputtop, "Propagator/ForwardProp", prop_header);
	read(inputtop, "Propagator/PropSource", source_header);
      }
      catch (const string& e) 
      {
	QDPIO::cerr << "Error extracting source_header: " << e << endl;
	throw;
      }

      {
	XMLBufferWriter prop_out_file_xml;
	push(prop_out_file_xml, "propagator");
	int id = 0;    // NEED TO FIX THIS - SOMETHING NON-TRIVIAL NEEDED
	write(prop_out_file_xml, "id", id);
	pop(prop_out_file_xml);

	XMLBufferWriter prop_out_record_xml;
	push(prop_out_record_xml, "Propagator");
	write(prop_out_record_xml, "ForwardProp", prop_header);
	write(prop_out_record_xml, "PropSource", source_header);
	write(prop_out_record_xml, "Config_info", gauge_xml);
	pop(prop_out_record_xml);
    
	// Write the source
	writeQprop(prop_out_file_xml, prop_out_record_xml, prop,
		   input.prop.prop_out_file, input.prop.prop_out_volfmt, 
		   QDPIO_SERIAL);
      }
    }
    break;

    case SCIDAC_SEQPROP:
    {
      // Try to invert this record XML into a source struct
      // Also pull out the id of this source
      SequentialProp_t sequential_prop_header;

      try
      {
	read(inputtop, "SequentialProp", sequential_prop_header);
      }
      catch (const string& e) 
      {
	QDPIO::cerr << "Error extracting sequential prop header: " << e << endl;
	throw;
      }

      {
	XMLBufferWriter prop_out_file_xml;
	push(prop_out_file_xml, "seqprop");
	int id = 0;    // NEED TO FIX THIS - SOMETHING NON-TRIVIAL NEEDED
	write(prop_out_file_xml, "id", id);
	pop(prop_out_file_xml);

	XMLBufferWriter prop_out_record_xml;
	push(prop_out_record_xml, "SequentialProp");
	write(prop_out_record_xml, "SeqProp", sequential_prop_header.seqprop_header);
	write(prop_out_record_xml, "SeqSourceSinkSmear", sequential_prop_header.sink_header);
	write(prop_out_record_xml, "SeqSource", sequential_prop_header.seqsource_header);
	write(prop_out_record_xml, "ForwardProps", sequential_prop_header.forward_props);
	write(prop_out_record_xml, "Config_info", gauge_xml);
	pop(prop_out_record_xml);
    
	// Write the source
	writeQprop(prop_out_file_xml, prop_out_record_xml, prop,
		   input.prop.prop_out_file, input.prop.prop_out_volfmt, 
		   QDPIO_SERIAL);
      }
    }
    break;

    default:
      QDPIO::cerr << "Unknown SciDAC prop type" << endl;
      QDP_abort(1);
    }
  }
  break;

  default:
    QDPIO::cerr << "unknown output type = " << input.prop.prop_out_type << endl;
    QDP_abort(1);
  }

  pop(xml_out);   // qproptransf
        
  xml_out.close();
  xml_in.close();

  END_CODE("qproptransf");

  // Time to bolt
  QDP_finalize();

  exit(0);
}
