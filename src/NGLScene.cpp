#include <QMouseEvent>
#include <QGuiApplication>

#include "NGLScene.h"
#include <ngl/Camera.h>
#include <ngl/Light.h>
#include <ngl/Material.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/ShaderLib.h>


//----------------------------------------------------------------------------------------------------------------------
/// @brief the increment for x/y translation with mouse movement
//----------------------------------------------------------------------------------------------------------------------
const static float INCREMENT=0.01f;
//----------------------------------------------------------------------------------------------------------------------
/// @brief the increment for the wheel zoom
//----------------------------------------------------------------------------------------------------------------------
const static float ZOOM=0.1f;

NGLScene::NGLScene(QWindow *_parent) : OpenGLWindow(_parent)
{
  // re-size the widget to that of the parent (in this case the GLFrame passed in on construction)
  m_rotate=false;
  // mouse rotation values set to 0
  m_spinXFace=0.0f;
  m_spinYFace=0.0f;
  setTitle("MVP Matrices");
  m_view.identity();
  m_project.identity();
  m_scale.set(1.0,1.0,1.0);
  m_wireframe=false;
 
}


NGLScene::~NGLScene()
{
  ngl::NGLInit *Init = ngl::NGLInit::instance();
  std::cout<<"Shutting down NGL, removing VAO's and Shaders\n";
  delete m_light;
  Init->NGLQuit();
}

void NGLScene::resizeEvent(QResizeEvent *_event )
{
  if(isExposed())
  {
  // set the viewport for openGL we need to take into account retina display
  // etc by using the pixel ratio as a multiplyer
  glViewport(0,0,width()*devicePixelRatio(),height()*devicePixelRatio());
  // now set the camera size values as the screen size has changed
  m_cam->setShape(45.0f,(float)width()/height(),0.05f,350.0f);
  renderLater();
  }
}


void NGLScene::initialize()
{
  // we must call this first before any other GL commands to load and link the
  // gl commands from the lib, if this is not done program will crash
  ngl::NGLInit::instance();

  glClearColor(0.4f, 0.4f, 0.4f, 1.0f);			   // Grey Background
  // enable depth testing for drawing
  glEnable(GL_DEPTH_TEST);
  // enable multisampling for smoother drawing
  glEnable(GL_MULTISAMPLE);
   // now to load the shader and set the values
  // grab an instance of shader manager
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  // we are creating a shader called Phong
  shader->createShaderProgram("Phong");
  // now we are going to create empty shaders for Frag and Vert
  shader->attachShader("PhongVertex",ngl::VERTEX);
  shader->attachShader("PhongFragment",ngl::FRAGMENT);
  // attach the source
  shader->loadShaderSource("PhongVertex","shaders/PhongVertex.glsl");
  shader->loadShaderSource("PhongFragment","shaders/PhongFragment.glsl");
  // compile the shaders
  shader->compileShader("PhongVertex");
  shader->compileShader("PhongFragment");
  // add them to the program
  shader->attachShaderToProgram("Phong","PhongVertex");
  shader->attachShaderToProgram("Phong","PhongFragment");
  // now bind the shader attributes for most NGL primitives we use the following
  // layout attribute 0 is the vertex data (x,y,z)
  shader->bindAttribute("Phong",0,"inVert");
  // attribute 1 is the UV data u,v (if present)
  shader->bindAttribute("Phong",1,"inUV");
  // attribute 2 are the normals x,y,z
  shader->bindAttribute("Phong",2,"inNormal");

  // now we have associated this data we can link the shader
  shader->linkProgramObject("Phong");
  // and make it active ready to load values
  (*shader)["Phong"]->use();
  // the shader will use the currently active material and light0 so set them
  ngl::Material m(ngl::GOLD);
  // load our material values to the shader into the structure material (see Vertex shader)
  m.loadToShader("material");
  // Now we will create a basic Camera from the graphics library
  // This is a static camera so it only needs to be set once
  // First create Values for the camera position
  ngl::Vec3 from(0,1,1);
  ngl::Vec3 to(0,0,0);
  ngl::Vec3 up(0,1,0);
  // now load to our new camera
  m_cam= new ngl::Camera(from,to,up);
  // set the shape using FOV 45 Aspect Ratio based on Width and Height
  // The final two are near and far clipping planes of 0.5 and 10
  m_cam->setShape(45.0f,(float)720.0/576.0f,0.05f,350.0f);
  shader->setShaderParam3f("viewerPos",m_cam->getEye().m_x,m_cam->getEye().m_y,m_cam->getEye().m_z);
  // now create our light this is done after the camera so we can pass the
  // transpose of the projection matrix to the light to do correct eye space
  // transformations
  ngl::Mat4 iv=m_cam->getViewMatrix();
  iv.transpose();
  m_light = new ngl::Light(ngl::Vec3(-2,5,2),ngl::Colour(1,1,1,1),ngl::Colour(1,1,1,1),ngl::POINTLIGHT );
  m_light->setTransform(iv);
  // load these values to the shader as well
  m_light->loadToShader("light");
  // as re-size is not explicitly called we need to do this.
  // set the viewport for openGL we need to take into account retina display
  // etc by using the pixel ratio as a multiplyer
  glViewport(0,0,width()*devicePixelRatio(),height()*devicePixelRatio());
  m_text = new  ngl::Text(QFont("Arial",18));
  m_text->setScreenSize(this->size().width(),this->size().height());


}


void NGLScene::loadMatricesToShader()
{
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();

  ngl::Mat4 MV;
  ngl::Mat3 normalMatrix;
  ngl::Mat4 M;

  M=m_scaleMat*m_rZ*m_rY*m_rX*m_tranMat;
  MV=  M*m_view;
  m_MVP= MV*m_project;
  normalMatrix=MV;
  normalMatrix.inverse();
  shader->setShaderParamFromMat4("MV",MV);
  shader->setShaderParamFromMat4("MVP",m_MVP);
  shader->setShaderParamFromMat3("normalMatrix",normalMatrix);
  shader->setShaderParamFromMat4("M",M);
}

void NGLScene::render()
{
  // clear the screen and depth buffer
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // grab an instance of the shader manager
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  (*shader)["Phong"]->use();

  // Rotation based on the mouse position for our global transform
  ngl::Mat4 rotX;
  ngl::Mat4 rotY;
  // create the rotation matrices
  rotX.rotateX(m_spinXFace);
  rotY.rotateY(m_spinYFace);
  // multiply the rotations
  m_mouseGlobalTX=rotY*rotX;
  // add the translations
  m_mouseGlobalTX.m_m[3][0] = m_modelPos.m_x;
  m_mouseGlobalTX.m_m[3][1] = m_modelPos.m_y;
  m_mouseGlobalTX.m_m[3][2] = m_modelPos.m_z;
  m_scaleMat.scale(m_scale.m_x,m_scale.m_y,m_scale.m_z);
  m_tranMat.translate(m_pos.m_x,m_pos.m_y,m_pos.m_z);
  m_rX.rotateX(m_rot.m_x);
  m_rY.rotateY(m_rot.m_y);
  m_rZ.rotateZ(m_rot.m_z);


   // get the VBO instance and draw the built in teapot
  ngl::VAOPrimitives *prim=ngl::VAOPrimitives::instance();
  // draw
  glPolygonMode(GL_FRONT_AND_BACK,m_wireframe ? GL_LINE : GL_FILL);

  loadMatricesToShader();
  prim->draw("bunny");
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  QString text;
  int tp=10;
  m_text->setColour(1.0,1.0,1.0);
  text.sprintf("MVP Matrix");
  m_text->renderText(tp,18*1,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_MVP.m_openGL[0],m_MVP.m_openGL[4],m_MVP.m_openGL[8],m_MVP.m_openGL[12]);
  m_text->renderText(tp,18*2,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_MVP.m_openGL[1],m_MVP.m_openGL[5],m_MVP.m_openGL[9],m_MVP.m_openGL[13]);
  m_text->renderText(tp,18*3,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_MVP.m_openGL[2],m_MVP.m_openGL[6],m_MVP.m_openGL[10],m_MVP.m_openGL[14]);
  m_text->renderText(tp,18*4,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_MVP.m_openGL[3],m_MVP.m_openGL[7],m_MVP.m_openGL[11],m_MVP.m_openGL[15]);
  m_text->renderText(tp,18*5,text );

  ngl::Mat4 M=m_scaleMat*m_rZ*m_rY*m_rX*m_tranMat;
  tp=700;
  text.sprintf("Model Matrix");
  m_text->renderText(tp,18*1,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",M.m_openGL[0],M.m_openGL[4],M.m_openGL[8],M.m_openGL[12]);
  m_text->renderText(tp,18*2,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",M.m_openGL[1],M.m_openGL[5],M.m_openGL[9],M.m_openGL[13]);
  m_text->renderText(tp,18*3,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",M.m_openGL[2],M.m_openGL[6],M.m_openGL[10],M.m_openGL[14]);
  m_text->renderText(tp,18*4,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",M.m_openGL[3],M.m_openGL[7],M.m_openGL[11],M.m_openGL[15]);
  m_text->renderText(tp,18*5,text );

  tp=10;
  text.sprintf("View Matrix");
  m_text->renderText(tp,18*34,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_view.m_openGL[0],m_view.m_openGL[4],m_view.m_openGL[8],m_view.m_openGL[12]);
  m_text->renderText(tp,18*35,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_view.m_openGL[1],m_view.m_openGL[5],m_view.m_openGL[9],m_view.m_openGL[13]);
  m_text->renderText(tp,18*36,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_view.m_openGL[2],m_view.m_openGL[6],m_view.m_openGL[10],m_view.m_openGL[14]);
  m_text->renderText(tp,18*37,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_view.m_openGL[3],m_view.m_openGL[7],m_view.m_openGL[11],m_view.m_openGL[15]);
  m_text->renderText(tp,18*38,text );



  tp=700;
  text.sprintf("Projection Matrix");
  m_text->renderText(tp,18*34,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_project.m_openGL[0],m_project.m_openGL[4],m_project.m_openGL[8],m_project.m_openGL[12]);
  m_text->renderText(tp,18*35,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_project.m_openGL[1],m_project.m_openGL[5],m_project.m_openGL[9],m_project.m_openGL[13]);
  m_text->renderText(tp,18*36,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_project.m_openGL[2],m_project.m_openGL[6],m_project.m_openGL[10],m_project.m_openGL[14]);
  m_text->renderText(tp,18*37,text );
  text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",m_project.m_openGL[3],m_project.m_openGL[7],m_project.m_openGL[11],m_project.m_openGL[15]);
  m_text->renderText(tp,18*38,text );


}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mouseMoveEvent (QMouseEvent * _event)
{
  // note the method buttons() is the button state when event was called
  // this is different from button() which is used to check which button was
  // pressed when the mousePress/Release event is generated
  if(m_rotate && _event->buttons() == Qt::LeftButton)
  {
    int diffx=_event->x()-m_origX;
    int diffy=_event->y()-m_origY;
    m_spinXFace += (float) 0.5f * diffy;
    m_spinYFace += (float) 0.5f * diffx;
    m_origX = _event->x();
    m_origY = _event->y();
    renderLater();

  }
        // right mouse translate code
  else if(m_translate && _event->buttons() == Qt::RightButton)
  {
    int diffX = (int)(_event->x() - m_origXPos);
    int diffY = (int)(_event->y() - m_origYPos);
    m_origXPos=_event->x();
    m_origYPos=_event->y();
    m_modelPos.m_x += INCREMENT * diffX;
    m_modelPos.m_y -= INCREMENT * diffY;
    renderLater();

   }
}


//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mousePressEvent ( QMouseEvent * _event)
{
  // this method is called when the mouse button is pressed in this case we
  // store the value where the maouse was clicked (x,y) and set the Rotate flag to true
  if(_event->button() == Qt::LeftButton)
  {
    m_origX = _event->x();
    m_origY = _event->y();
    m_rotate =true;
  }
  // right mouse translate mode
  else if(_event->button() == Qt::RightButton)
  {
    m_origXPos = _event->x();
    m_origYPos = _event->y();
    m_translate=true;
  }

}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mouseReleaseEvent ( QMouseEvent * _event )
{
  // this event is called when the mouse button is released
  // we then set Rotate to false
  if (_event->button() == Qt::LeftButton)
  {
    m_rotate=false;
  }
        // right mouse translate mode
  if (_event->button() == Qt::RightButton)
  {
    m_translate=false;
  }
}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::wheelEvent(QWheelEvent *_event)
{

	// check the diff of the wheel position (0 means no change)
	if(_event->delta() > 0)
	{
		m_modelPos.m_z+=ZOOM;
	}
	else if(_event->delta() <0 )
	{
		m_modelPos.m_z-=ZOOM;
	}
	renderLater();
}
//----------------------------------------------------------------------------------------------------------------------

void NGLScene::keyPressEvent(QKeyEvent *_event)
{
  // this method is called every time the main window recives a key event.
  // we then switch on the key value and set the camera in the GLWindow
  switch (_event->key())
  {
  // escape key to quit
  case Qt::Key_Escape : QGuiApplication::exit(EXIT_SUCCESS); break;
  // turn on wirframe rendering
  case Qt::Key_W : m_wireframe=true; break;
  // turn off wire frame
  case Qt::Key_S : m_wireframe=false; break;
  // show full screen
  case Qt::Key_F : showFullScreen(); break;
  // show windowed
  case Qt::Key_N : showNormal(); break;
  // position
  case Qt::Key_Up : m_pos.m_y+=0.1f; break;
  case Qt::Key_Down : m_pos.m_y-=0.1f; break;
  case Qt::Key_Left : m_pos.m_x-=0.1f; break;
  case Qt::Key_Right : m_pos.m_x+=0.1f; break;
  case Qt::Key_I : m_pos.m_z-=0.1f; break;
  case Qt::Key_O : m_pos.m_z+=0.1f; break;

  case Qt::Key_1 : m_rot.m_x+=2.0; break;
  case Qt::Key_2 : m_rot.m_x-=2.0; break;
  case Qt::Key_3 : m_rot.m_y+=2.0; break;
  case Qt::Key_4 : m_rot.m_y-=2.0; break;
  case Qt::Key_5 : m_rot.m_z+=2.0; break;
  case Qt::Key_6 : m_rot.m_z-=2.0; break;


  case Qt::Key_8 : m_scale.m_x+=0.1; break;
  case Qt::Key_9 : m_scale.m_y+=0.1; break;
  case Qt::Key_0 : m_scale.m_z+=0.1; break;
  case Qt::Key_Minus : m_scale.m_x-=0.1; break;
  case Qt::Key_Equal : m_scale.m_y-=0.1; break;
  case Qt::Key_Backspace : m_scale.m_z-=0.1; break;


  case Qt::Key_P :
    m_project=ngl::perspective(35.0,float(width()/height()),0.1,500);
  break;
  case Qt::Key_M :
    m_project=ngl::ortho(-10,10,-10,10,10,-10);
  break;
  case Qt::Key_V :
    m_view=ngl::lookAt(ngl::Vec3(0,0,2),ngl::Vec3(0,0,0),ngl::Vec3(0,1,0));
  break;
  case Qt::Key_Space :
    m_project.identity();
    m_view.identity();
    m_rot.set(0.0,0.0,0.0);
    m_pos.set(0.0,0.0,0.0);
    m_scaleMat.identity();
    m_tranMat.identity();
    m_scale.set(1.0,1.0,1.0);
  break;
  default : break;
  }
  // finally update the GLWindow and re-draw
  //if (isExposed())
    renderLater();
}