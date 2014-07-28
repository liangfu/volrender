/*=========================================================================

  Program:   Visualization Toolkit
  Module:    FixedPointVolumeRayCastMapperCT.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

  =========================================================================*/
// VTK includes
#include "vtkBoxWidget.h"
#include "vtkCamera.h"
#include "vtkCommand.h"
#include "vtkColorTransferFunction.h"
#include "vtkDICOMImageReader.h"
#include "vtkImageData.h"
#include "vtkImageResample.h"
#include "vtkMetaImageReader.h"
#include "vtkPiecewiseFunction.h"
#include "vtkPlanes.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkVolume.h"
#include "vtkVolumeProperty.h"
#include "vtkXMLImageDataReader.h"
#include "vtkFixedPointVolumeRayCastMapper.h"

#include "vtkInteractorStyleTrackballCamera.h"
#include "vtkSmartPointer.h"
#include "vtkWindowToImageFilter.h"
#include "vtkPNGWriter.h"

#define VTI_FILETYPE 1
#define MHA_FILETYPE 2

void PrintUsage();

int main(int argc, char *argv[])
{
  // Parse the parameters

  int count = 1;
  char *dirname = NULL;
  double opacityWindow = 4096;
  double opacityLevel = 2048;
  int blendType = 0;
  int clip = 0;
  double reductionFactor = 1.0;
  double frameRate = 10.0;
  char *fileName=0;
  int fileType=0;

  bool independentComponents=true;

  while ( count < argc )
  {
    if ( !strcmp( argv[count], "?" ) )
	{
      PrintUsage();
      exit(EXIT_SUCCESS);
	}
    else if ( !strcmp( argv[count], "-MHA" ) )
	{
      fileName = new char[strlen(argv[count+1])+1];
      fileType = MHA_FILETYPE;
      sprintf( fileName, "%s", argv[count+1] );
      count += 2;
	}
    else if ( !strcmp( argv[count], "-Jet" ) )
	{
      blendType = 7;
      count += 1;
	}
    else if ( !strcmp( argv[count], "-FrameRate") )
	{
      frameRate = atof( argv[count+1] );
      if ( frameRate < 0.01 || frameRate > 60.0 )
	  {
        cout << "Invalid frame rate - use a number between 0.01 and 60.0" << endl;
        cout << "Using default frame rate of 10 frames per second." << endl;
        frameRate = 30.0;
	  }
      count += 2;
	}
    else if ( !strcmp( argv[count], "-ReductionFactor") )
	{
      reductionFactor = atof( argv[count+1] );
      if ( reductionFactor <= 0.0 || reductionFactor >= 1.0 )
	  {
        cout << "Invalid reduction factor - use a number between 0 and 1 (exclusive)" << endl;
        cout << "Using the default of no reduction." << endl;
        reductionFactor = 1.0;
	  }
      count += 2;
	}
	else if ( !strcmp( argv[count], "-DependentComponents") )
	{
      independentComponents=false;
      count += 1;
	}
    else
	{
      cout << "Unrecognized option: " << argv[count] << endl;
      cout << endl;
      PrintUsage();
      exit(EXIT_FAILURE);
	}
  }

  if ( !dirname && !fileName)
  {
    cout << "Error: you must specify a directory of DICOM data or a .vti file or a .mha!" << endl;
    cout << endl;
    PrintUsage();
    exit(EXIT_FAILURE);
  }

  // Create the renderer, render window and interactor
  vtkRenderer *renderer = vtkRenderer::New();
  vtkRenderWindow *renWin = vtkRenderWindow::New();
  renWin->AddRenderer(renderer);
  renderer->SetBackground(1,1,1);

  // Connect it all. Note that funny arithematic on the
  // SetDesiredUpdateRate - the vtkRenderWindow divides it
  // allocated time across all renderers, and the renderer
  // divides it time across all props. If clip is
  // true then there are two props
  vtkRenderWindowInteractor *iren = vtkRenderWindowInteractor::New();
  iren->SetRenderWindow(renWin);
  iren->SetDesiredUpdateRate(frameRate / (1+clip) );

  //iren->GetInteractorStyle()->SetDefaultRenderer(renderer);
  vtkInteractorStyleTrackballCamera *style = 
    vtkInteractorStyleTrackballCamera::New();
  iren->SetInteractorStyle(style);
  style->SetDefaultRenderer(renderer);

  // Read the data
  vtkAlgorithm *reader=0;
  vtkImageData *input=0;
  if(dirname)
  {
    vtkDICOMImageReader *dicomReader = vtkDICOMImageReader::New();
    dicomReader->SetDirectoryName(dirname);
    dicomReader->Update();
    input=dicomReader->GetOutput();
    reader=dicomReader;
  }
  else if ( fileType == MHA_FILETYPE )
  {
    vtkMetaImageReader *metaReader = vtkMetaImageReader::New();
    metaReader->SetFileName(fileName);
    metaReader->Update();
    input=metaReader->GetOutput();
    reader=metaReader;
  }
  else
  {
    cout << "Error! Not VTI or MHA!" << endl;
    exit(EXIT_FAILURE);
  }

  // Verify that we actually have a volume
  int dim[3];
  input->GetDimensions(dim);
  if ( dim[0] < 2 ||
       dim[1] < 2 ||
       dim[2] < 2 )
  {
    cout << "Error loading data!" << endl;
    exit(EXIT_FAILURE);
  }

  vtkImageResample *resample = vtkImageResample::New();
  if ( reductionFactor < 1.0 )
  {
    resample->SetInputConnection( reader->GetOutputPort() );
    resample->SetAxisMagnificationFactor(0, reductionFactor);
    resample->SetAxisMagnificationFactor(1, reductionFactor);
    resample->SetAxisMagnificationFactor(2, reductionFactor);
  }

  // Create our volume and mapper
  vtkVolume *volume = vtkVolume::New();
  vtkFixedPointVolumeRayCastMapper *mapper = vtkFixedPointVolumeRayCastMapper::New();

  if ( reductionFactor < 1.0 )
  {
    mapper->SetInputConnection( resample->GetOutputPort() );
  }
  else
  {
    mapper->SetInputConnection( reader->GetOutputPort() );
  }

  // Set the sample distance on the ray to be 1/2 the average spacing
  double spacing[3];
  if ( reductionFactor < 1.0 )
  {
    resample->GetOutput()->GetSpacing(spacing);
  }
  else
  {
    input->GetSpacing(spacing);
  }

  //  mapper->SetSampleDistance( (spacing[0]+spacing[1]+spacing[2])/6.0 );
  //  mapper->SetMaximumImageSampleDistance(10.0);

  // Create our transfer function
  vtkColorTransferFunction *colorFun = vtkColorTransferFunction::New();
  vtkPiecewiseFunction *opacityFun = vtkPiecewiseFunction::New();

  // Create the property and attach the transfer functions
  vtkVolumeProperty *property = vtkVolumeProperty::New();
  property->SetIndependentComponents(independentComponents);
  property->SetColor( colorFun );
  property->SetScalarOpacity( opacityFun );
  property->SetInterpolationTypeToLinear();

  // connect up the volume to the property and the mapper
  volume->SetProperty( property );
  volume->SetMapper( mapper );

  // Depending on the blend type selected as a command line option,
  // adjust the transfer function
  switch ( blendType )
  {
	// Grayscale color map for negative
	// Jet color map for positive
  case 7:
	colorFun->AddRGBPoint( -3024, 1, 1, 1 );
	colorFun->AddRGBPoint( -1,    1, 1, 1 );
	colorFun->AddRGBPoint( 1,    .0, .0, .5 );
	// colorFun->AddRGBPoint( 1000, 1, .0, 0 );
	colorFun->AddRGBPoint( 1400, .0, .0, 0.5 );
	colorFun->AddRGBPoint( 1460, 0, .5, 1 );
	colorFun->AddRGBPoint( 1530, 1, .5,  0 );
	colorFun->AddRGBPoint( 1601, 1,  0,  0 );
	colorFun->AddRGBPoint( 3071, 1,  0,  0 );

	opacityFun->AddPoint(-3024, 0.0 );
	opacityFun->AddPoint(-3000, 0.0 );
	opacityFun->AddPoint(-2999, 0.01 );
	opacityFun->AddPoint(-2044, 0.02 );
	opacityFun->AddPoint(-2000, 0.0 );
	opacityFun->AddPoint(-1,    0.0 );
	opacityFun->AddPoint(2,    0.0 );
	opacityFun->AddPoint(1400, .1 );
	opacityFun->AddPoint(1600, .1 );
	opacityFun->AddPoint(3071, .0 );

	mapper->SetBlendModeToComposite();
	property->ShadeOn();
	property->SetAmbient(0.1);
	property->SetDiffuse(0.9);
	property->SetSpecular(0.2);
	property->SetSpecularPower(10.0);
	property->SetScalarOpacityUnitDistance(0.8919);
	break;

  default:
	vtkGenericWarningMacro("Unknown blend type.");
	break;
  }

  // Set the default window size
  renWin->SetSize(600,600);
  // renWin->Render();

  // Add the volume to the scene
  renderer->AddVolume( volume );
  renderer->ResetCamera();

  // interact with data
  //renWin->Render();

  renderer->GetActiveCamera()->Pitch(90);
  renderer->ResetCamera();
  renderer->GetActiveCamera()->Azimuth(90);
  renderer->ResetCamera();

#if 1
  int angle;
  char foldername[1024]={0,};
  char outimgname[1024]={0,};
  int outimgslashloc=-1;
  for (int i=strlen(fileName)-1;i>1;i--){
	if (fileName[i]=='/'){outimgslashloc=i;break;}
  }
  fprintf(stderr,"%s\n",fileName);
  strncpy(foldername,fileName,outimgslashloc);
  for (angle=0;angle<360;angle+=10)
  {
	sprintf(outimgname,"%s/%03d.png",foldername,angle);
	fprintf(stderr,"%s\n",outimgname);
	renderer->GetActiveCamera()->Azimuth(-10);
	renderer->ResetCamera();
	// renWin->Render();
  
	// Screenshot
	vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter =
	  vtkSmartPointer<vtkWindowToImageFilter>::New();
	windowToImageFilter->SetInput(renWin);
	//windowToImageFilter->SetMagnification(1); //set the resolution of the output image
	windowToImageFilter->SetViewport(.25,.32,.75,.72);
	windowToImageFilter->Update();

	vtkSmartPointer<vtkPNGWriter> writer =
	  vtkSmartPointer<vtkPNGWriter>::New();
	writer->SetFileName(outimgname);
	writer->SetInputConnection(windowToImageFilter->GetOutputPort());
	writer->Write();
	//if (angle==20){break;}
  }
#else
  renWin->Render();
  
  // start renderer
  iren->Start();
#endif

  opacityFun->Delete();
  colorFun->Delete();
  property->Delete();

  volume->Delete();
  mapper->Delete();
  reader->Delete();
  resample->Delete();
  renderer->Delete();
  style->Delete();
  renWin->Delete();
  iren->Delete();

  return 0;
}

void PrintUsage()
{
  cout << "Usage: " << endl;
  cout << endl;
  cout << "  FixedPointVolumeRayCastMapperCT <options>" << endl;
  cout << endl;
  cout << "where options may include: " << endl;
  cout << endl;
  // cout << "  -DICOM <directory>" << endl;
  // cout << "  -VTI <filename>" << endl;
  cout << "  -MHA <filename>" << endl;
  // cout << "  -DependentComponents" << endl;
  // cout << "  -Clip" << endl;
  // cout << "  -MIP <window> <level>" << endl;
  // cout << "  -CompositeRamp <window> <level>" << endl;
  // cout << "  -CompositeShadeRamp <window> <level>" << endl;
  // cout << "  -CT_Skin" << endl;
  // cout << "  -CT_Bone" << endl;
  // cout << "  -CT_Muscle" << endl;
  cout << "  -Jet" << endl;
  cout << "  -FrameRate <rate>" << endl;
  // cout << "  -DataReduction <factor>" << endl;
  cout << endl;
  cout << "You must use either the -DICOM option to specify the directory where" << endl;
  cout << "the data is located or the -VTI or -MHA option to specify the path of a .vti file." << endl;
  cout << endl;
  cout << "By default, the program assumes that the file has independent components," << endl;
  cout << "use -DependentComponents to specify that the file has dependent components." << endl;
  cout << endl;
  cout << "Use the -Clip option to display a cube widget for clipping the volume." << endl;
  cout << "Use the -FrameRate option with a desired frame rate (in frames per second)" << endl;
  cout << "which will control the interactive rendering rate." << endl;
  cout << "Use the -DataReduction option with a reduction factor (greater than zero and" << endl;
  cout << "less than one) to reduce the data before rendering." << endl;
  cout << "Use one of the remaining options to specify the blend function" << endl;
  cout << "and transfer functions. The -MIP option utilizes a maximum intensity" << endl;
  cout << "projection method, while the others utilize compositing. The" << endl;
  cout << "-CompositeRamp option is unshaded compositing, while the other" << endl;
  cout << "compositing options employ shading." << endl;
  cout << endl;
  cout << "Note: MIP, CompositeRamp, CompositeShadeRamp, CT_Skin, CT_Bone," << endl;
  cout << "and CT_Muscle are appropriate for DICOM data. MIP, CompositeRamp," << endl;
  cout << "and RGB_Composite are appropriate for RGB data." << endl;
  cout << endl;
  cout << "Example: FixedPointVolumeRayCastMapperCT -DICOM CTNeck -MIP 4096 1024" << endl;
  cout << endl;
}

