#include <QMouseEvent>
#include <QGuiApplication>

#include "NGLScene.h"
#include <ngl/NGLInit.h>
#include <ngl/NGLStream.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/ShaderLib.h>
#include <ngl/MultiBufferVAO.h>

const  std::array<ngl::Vec3,3> NGLScene::s_triVerts=
      {{
        ngl::Vec3(-0.0f,0.5f,0.0f),
        ngl::Vec3(0.5f,-0.5f,0.0f),
        ngl::Vec3(-0.5f,-0.5f,0.0f)
       }};


NGLScene::NGLScene()
{
  setTitle("MVP Matrices");
  m_view.identity();
  m_project.identity();
  m_scale.set(1.0,1.0,1.0);
  m_wireframe=false;
 
}

void NGLScene::createTriangle()
{

  std::vector <ngl::Vec3> normals;
  ngl::Vec3 n(0.0f,1.0f,0.0f);
    normals.push_back(n);
    normals.push_back(n);
    normals.push_back(n);
    // create a vao as a series of GL_TRIANGLES
    m_tri= ngl::VAOFactory::createVAO(ngl::multiBufferVAO,GL_TRIANGLES);
    m_tri->bind();

    // in this case we are going to set our data as the vertices above

    m_tri->setData(ngl::AbstractVAO::VertexData(sizeof(s_triVerts),s_triVerts[0].m_x));
    // now we set the attribute pointer to be 0 (as this matches vertIn in our shader)

    m_tri->setVertexAttributePointer(0,3,GL_FLOAT,0,0);

    m_tri->setData(ngl::AbstractVAO::VertexData(sizeof(s_triVerts),normals[0].m_x));
    // now we set the attribute pointer to be 1 (as this matches normal in our shader)

    m_tri->setVertexAttributePointer(1,3,GL_FLOAT,0,0);

    m_tri->setNumIndices(sizeof(s_triVerts)/sizeof(ngl::Vec3));

   // now unbind
    m_tri->unbind();




}

NGLScene::~NGLScene()
{
  std::cout<<"Shutting down NGL, removing VAO's and Shaders\n";
}



void NGLScene::resizeGL( int _w, int _h )
{
  m_view=ngl::perspective( 45.0f, static_cast<float>( _w ) / _h, 0.05f, 350.0f );
  m_win.width  = static_cast<int>( _w * devicePixelRatio() );
  m_win.height = static_cast<int>( _h * devicePixelRatio() );
}

void NGLScene::initializeGL()
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
  shader->attachShader("PhongVertex",ngl::ShaderType::VERTEX);
  shader->attachShader("PhongFragment",ngl::ShaderType::FRAGMENT);
  // attach the source
  shader->loadShaderSource("PhongVertex","shaders/PhongVertex.glsl");
  shader->loadShaderSource("PhongFragment","shaders/PhongFragment.glsl");
  // compile the shaders
  shader->compileShader("PhongVertex");
  shader->compileShader("PhongFragment");
  // add them to the program
  shader->attachShaderToProgram("Phong","PhongVertex");
  shader->attachShaderToProgram("Phong","PhongFragment");

  // now we have associated this data we can link the shader
  shader->linkProgramObject("Phong");
  // and make it active ready to load values
  (*shader)["Phong"]->use();
  // Now we will create a basic Camera from the graphics library
  // This is a static camera so it only needs to be set once
  // First create Values for the camera position
  ngl::Vec3 from(0,1,1);
  ngl::Vec3 to(0,0,0);
  ngl::Vec3 up(0,1,0);
  // now load to our new camera
  m_view=ngl::lookAt(from,to,up);
  // set the shape using FOV 45 Aspect Ratio based on Width and Height
  // The final two are near and far clipping planes of 0.5 and 10
  m_project=ngl::perspective(45.0f,720.0f/576.0f,0.05f,350.0f);
  shader->setUniform("viewerPos",from);
  ngl::Vec4 lightPos(0.0f,2.0f,2.0f,0.0f);
  shader->setUniform("light.position",lightPos);
  shader->setUniform("light.ambient",0.0f,0.0f,0.0f,1.0f);
  shader->setUniform("light.diffuse",1.0f,1.0f,1.0f,1.0f);
  shader->setUniform("light.specular",0.8f,0.8f,0.8f,1.0f);
  // gold like phong material
  shader->setUniform("material.ambient",0.274725f,0.1995f,0.0745f,0.0f);
  shader->setUniform("material.diffuse",0.75164f,0.60648f,0.22648f,0.0f);
  shader->setUniform("material.specular",0.628281f,0.555802f,0.3666065f,0.0f);
  shader->setUniform("material.shininess",51.2f);


  m_text.reset(  new  ngl::Text(QFont("Arial",18)));
  m_text->setScreenSize(width(),height());
  createTriangle();


}


void NGLScene::loadMatricesToShader()
{
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();

  ngl::Mat4 MV;
  ngl::Mat3 normalMatrix;
  ngl::Mat4 M;
  M=m_tranMat * m_rX * m_rY*m_rZ*m_scaleMat;
  MV=  m_view*M;
  m_MVP= m_project*MV;
  normalMatrix=MV;
  normalMatrix.inverse().transpose();
  shader->setUniform("MV",MV);
  shader->setUniform("MVP",m_MVP);
  shader->setUniform("normalMatrix",normalMatrix);
  shader->setUniform("M",M);
}

void NGLScene::paintGL()
{
  // clear the screen and depth buffer
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // grab an instance of the shader manager
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  (*shader)["Phong"]->use();


  m_scaleMat.scale(m_scale.m_x,m_scale.m_y,m_scale.m_z);
  m_tranMat.translate(m_pos.m_x,m_pos.m_y,m_pos.m_z);
  m_rX.rotateX(m_rot.m_x);
  m_rY.rotateY(m_rot.m_y);
  m_rZ.rotateZ(m_rot.m_z);


  // draw
  glPolygonMode(GL_FRONT_AND_BACK,m_wireframe ? GL_LINE : GL_FILL);

  loadMatricesToShader();
  //prim->draw("bunny");
  m_tri->bind();
  m_tri->draw();
  m_tri->unbind();
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

  tp=10;
  text.sprintf("Original Triangle Vertices");
  m_text->renderText(tp,18*10,text );
  int y=11;
  for(auto p : s_triVerts)
  {
    text.sprintf("[ %+0.4f %+0.4f %+0.4f +1.0]",p.m_x,p.m_y,p.m_z);
    m_text->renderText(tp,18*y++,text );

  }
  y++;
  m_text->setColour(1.0,0.0,0.0);

  text.sprintf("Transformed Triangle Vertices");

  m_text->renderText(tp,18*y++,text );
  m_text->setColour(1.0,1.0,0.0);
  for(auto p : s_triVerts)
  {
    ngl::Vec4 pt=m_MVP*p;
    std::cout<<"pt "<<pt<<M<<'\n';
    text.sprintf("[ %+0.4f %+0.4f %+0.4f %+0.4f]",pt.m_x,pt.m_y,pt.m_z,pt.m_w);
    m_text->renderText(tp,18*y++,text );

  }



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
    m_project=ngl::perspective(35.0,float(width()/height()),0.1f,500);
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
    update();
}
