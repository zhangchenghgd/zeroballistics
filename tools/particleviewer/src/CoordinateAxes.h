


#include <osg/Geode>
#include <osg/Drawable>
#include <osg/Shape>
#include <osg/ShapeDrawable>

#include <osgText/Text>

void setupCoordAxes( osg::Geode* geode )
{
        float height = 0.1f;
        float radius = 0.01f;

        osg::ref_ptr<osgText::Text> label;

        osg::ref_ptr<osg::Sphere> hub = new osg::Sphere;
        hub->setCenter( osg::Vec3(0.0f, 0.0f, 0.0f) );
        hub->setRadius( 1.5f*radius );
        osg::ref_ptr<osg::ShapeDrawable> hubDraw = new osg::ShapeDrawable( hub.get() );
        hubDraw->setColor( osg::Vec4(0.0f, 1.0f, 1.0f, 1.0f) ); // cyan
        geode->addDrawable( hubDraw.get() );

        ///####################   X Axis   #############################################
        osg::ref_ptr<osg::Cylinder> xaxis = new osg::Cylinder;
        xaxis->setCenter( osg::Vec3(0.5f*height, 0.0f, 0.0f) );
        xaxis->setHeight( height );
        xaxis->setRadius( radius );
        xaxis->setRotation( osg::Quat(osg::PI_2, osg::Vec3(0.0f,1.0f,0.0f)) );
        osg::ref_ptr<osg::ShapeDrawable> xaxisDraw = new osg::ShapeDrawable( xaxis.get() );
        xaxisDraw->setColor( osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f) ); // red
        geode->addDrawable( xaxisDraw.get() );

        osg::ref_ptr<osg::Sphere> xaxisEnd = new osg::Sphere;
        xaxisEnd->setCenter( osg::Vec3(height, 0.0f, 0.0f) );
        xaxisEnd->setRadius( 1.5f*radius );
        osg::ref_ptr<osg::ShapeDrawable> xaxisEndDraw = new osg::ShapeDrawable( xaxisEnd.get() );
        xaxisEndDraw->setColor( osg::Vec4(0.0f, 1.0f, 1.0f, 1.0f) ); // cyan
        geode->addDrawable( xaxisEndDraw.get() );

        ///####################   Label   #############################################
        label = new osgText::Text();
        label->setCharacterSize(0.05);
        label->setFont(FONT_PATH + "FreeMono.ttf");
        label->setText("X");
        label->setFontResolution(40,40);
        label->setAxisAlignment(osgText::Text::SCREEN);
        label->setDrawMode(osgText::Text::TEXT);
        label->setAlignment(osgText::Text::CENTER_TOP);
        label->setPosition( osg::Vec3(1.8 * height, 0, 0 ) );
        label->setColor( osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f) );        
        geode->addDrawable(label.get());



        ///####################   Y Axis   #############################################
        osg::ref_ptr<osg::Cylinder> yaxis = new osg::Cylinder;
        yaxis->setCenter( osg::Vec3(0.0f, 0.5f*height, 0.0f) );
        yaxis->setHeight( height );
        yaxis->setRadius( radius );
        yaxis->setRotation( osg::Quat(-osg::PI_2, osg::Vec3(1.0f,0.0f,0.0f)) );
        osg::ref_ptr<osg::ShapeDrawable> yaxisDraw = new osg::ShapeDrawable( yaxis.get() );
        yaxisDraw->setColor( osg::Vec4(0.0f, 1.0f, 0.0f, 1.0f) ); // green
        geode->addDrawable( yaxisDraw.get() );

        osg::ref_ptr<osg::Sphere> yaxisEnd = new osg::Sphere;
        yaxisEnd->setCenter( osg::Vec3(0.0f, height, 0.0f) );
        yaxisEnd->setRadius( 1.5f*radius );
        osg::ref_ptr<osg::ShapeDrawable> yaxisEndDraw = new osg::ShapeDrawable( yaxisEnd.get() );
        yaxisEndDraw->setColor( osg::Vec4(0.0f, 1.0f, 1.0f, 1.0f) ); // cyan
        geode->addDrawable( yaxisEndDraw.get() );


        ///####################   Label   #############################################
        label = new osgText::Text();
        label->setCharacterSize(0.05);
        label->setFont(FONT_PATH + "FreeMono.ttf");
        label->setText("Y");
        label->setFontResolution(40,40);
        label->setAxisAlignment(osgText::Text::SCREEN);
        label->setDrawMode(osgText::Text::TEXT);
        label->setAlignment(osgText::Text::CENTER_TOP);
        label->setPosition( osg::Vec3(0, 1.8 * height, 0 ) );
        label->setColor( osg::Vec4(0.0f, 1.0f, 0.0f, 1.0f) );        
        geode->addDrawable(label.get());


        ///####################   Z Axis   #############################################
        osg::ref_ptr<osg::Cylinder> zaxis = new osg::Cylinder;
        zaxis->setCenter( osg::Vec3(0.0f, 0.0f, 0.5f*height) );
        zaxis->setHeight( height );
        zaxis->setRadius( radius );
        osg::ref_ptr<osg::ShapeDrawable> zaxisDraw = new osg::ShapeDrawable( zaxis.get() );
        zaxisDraw->setColor( osg::Vec4(0.0f, 0.0f, 1.0f, 1.0f) ); // blue
        geode->addDrawable( zaxisDraw.get() );

        osg::ref_ptr<osg::Sphere> zaxisEnd = new osg::Sphere;
        zaxisEnd->setCenter( osg::Vec3(0.0f, 0.0f, height) );
        zaxisEnd->setRadius( 1.5f*radius );
        osg::ref_ptr<osg::ShapeDrawable> zaxisEndDraw = new osg::ShapeDrawable( zaxisEnd.get() );
        zaxisEndDraw->setColor( osg::Vec4(0.0f, 1.0f, 1.0f, 1.0f) ); // cyan
        geode->addDrawable( zaxisEndDraw.get() );


        ///####################   Label   #############################################
        label = new osgText::Text();
        label->setCharacterSize(0.05);
        label->setFont(FONT_PATH + "FreeMono.ttf");
        label->setText("Z");
        label->setFontResolution(40,40);
        label->setAxisAlignment(osgText::Text::SCREEN);
        label->setDrawMode(osgText::Text::TEXT);
        label->setAlignment(osgText::Text::CENTER_TOP);
        label->setPosition( osg::Vec3(0, 0, 1.8 * height ) );
        label->setColor( osg::Vec4(0.0f, 0.0f, 1.0f, 1.0f) );        
        geode->addDrawable(label.get());

}
