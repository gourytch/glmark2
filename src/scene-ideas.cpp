#include "scene.h"
#include "stack.h"
#include "splines.h"
#include "table.h"
#include "logo.h"
#include "lamp.h"
#include "util.h"
#include <sys/time.h>

using LibMatrix::Stack4;
using LibMatrix::mat4;
using LibMatrix::vec3;
using LibMatrix::vec4;
using LibMatrix::uvec3;

class SceneIdeasPrivate
{
public:
    SceneIdeasPrivate() :
        valid_(false),
//        paused_(false),
//        doTimerReset_(true),
//        doPostIdle_(false),
//        timeJerked_(false),
        currentTime_(START_TIME_),
//        holdTime_(0),
        timeOffset_(START_TIME_)
    {
        startTime_.tv_sec = 0;
        startTime_.tv_usec = 0; 
    }
    ~SceneIdeasPrivate()
    {
    }
    void initialize();
    void reset_time();
    void update_time();
    void update_projection(const mat4& proj);
    void draw();

private:
    void postIdle();
    void initLights();
    //OptionData od_;
    bool valid_;
    Stack4 projection_;
    Stack4 modelview_;
//    bool paused_;
//    bool doTimerReset_;
//    bool doPostIdle_;
//    bool timeJerked_; // Indicates radical time shift due to UI.
    float currentTime_;
//    float holdTime_;
    float timeOffset_;
    struct timeval startTime_;
    static const float TIME_;
    static const float START_TIME_;
    // Table
    Table table_;
    // Logo
    SGILogo logo_;
    // Lamp
    Lamp lamp_;
    // Light constants
    static const vec4 light0_position_;
    static const vec4 light1_position_;
    static const vec4 light2_position_;
    // Object constants
    ViewFromSpline viewFromSpline_;
    ViewToSpline viewToSpline_;
    LightPositionSpline lightPosSpline_;
    LogoPositionSpline logoPosSpline_;
    LogoRotationSpline logoRotSpline_;
    vec3 viewFrom_;
    vec3 viewTo_;
    vec3 lightPos_;
    vec3 logoPos_;
    vec3 logoRot_;
    vec4 lightPositions_[3];
};

const float SceneIdeasPrivate::TIME_(15.0);
const float SceneIdeasPrivate::START_TIME_(0.6);
const vec4 SceneIdeasPrivate::light0_position_(0.0, 1.0, 0.0, 0.0);
const vec4 SceneIdeasPrivate::light1_position_(-1.0, 0.0, 0.0, 0.0);
const vec4 SceneIdeasPrivate::light2_position_(0.0, -1.0, 0.0, 0.0);

void
SceneIdeasPrivate::initLights()
{
    const mat4& curMV(modelview_.getCurrent());
    lightPositions_[0] = curMV * light0_position_;
    lightPositions_[1] = curMV * light1_position_;
    lightPositions_[2] = curMV * light2_position_;
}

void
SceneIdeasPrivate::initialize()
{
    // Initialize the positions for the lights we'll use.
    initLights();

    // Tell the objects in the scene to initialize themselves.
    table_.init();
    if (!table_.valid())
    {
        return;
    }
    logo_.init();
    if (!logo_.valid())
    {
        return;
    }
    lamp_.init();
    if (!lamp_.valid())
    {
        return;
    }

    reset_time();

    // If we're here, we're okay to run.
    valid_ = true;
}

void
SceneIdeasPrivate::reset_time()
{
    timeOffset_ = START_TIME_;
    gettimeofday(&startTime_, NULL);
}

void
SceneIdeasPrivate::update_time()
{
    // Compute new time
    struct timeval current = {0, 0};
    gettimeofday(&current, NULL);
    float timediff = (current.tv_sec - startTime_.tv_sec) + 
        static_cast<double>(current.tv_usec - startTime_.tv_usec) / 1000000.0;
    currentTime_ = timediff + timeOffset_;
}

void
SceneIdeasPrivate::update_projection(const mat4& proj)
{
    // Projection hasn't changed since last frame.
    if (projection_.getCurrent() == proj)
    {
        return;
    }

    projection_.loadIdentity();
    projection_ *= proj;
}

SceneIdeas::SceneIdeas(Canvas& canvas) :
    Scene(canvas, "ideas")
{
    priv_ = new SceneIdeasPrivate();
}

SceneIdeas::~SceneIdeas()
{
    delete priv_;
}

bool
SceneIdeas::load()
{
    return true;
}

void
SceneIdeas::unload()
{
}

void
SceneIdeas::setup()
{
    Scene::setup();
    priv_->initialize();
    priv_->update_projection(canvas_.projection());
    running_ = true;
    startTime_ = Util::get_timestamp_us() / 1000000.0;
    lastUpdateTime_ = startTime_;
}

void
SceneIdeas::update()
{
    Scene::update();
    priv_->update_time();
    priv_->update_projection(canvas_.projection());
}

void
SceneIdeasPrivate::draw()
{
    viewFromSpline_.getCurrentVec(currentTime_, viewFrom_);
    viewToSpline_.getCurrentVec(currentTime_, viewTo_);
    lightPosSpline_.getCurrentVec(currentTime_, lightPos_);
    logoPosSpline_.getCurrentVec(currentTime_, logoPos_);
    logoRotSpline_.getCurrentVec(currentTime_, logoRot_);

    // Tell the logo its new position
    logo_.setPosition(logoPos_);

    vec4 lp4(lightPos_.x(), lightPos_.y(), lightPos_.z(), 0.0);

//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

    //
    // SHADOW
    //
    modelview_.loadIdentity();
    modelview_.lookAt(viewFrom_.x(), viewFrom_.y(), viewFrom_.z(), 
                      viewTo_.x(), viewTo_.y(), viewTo_.z(),
                      0.0, 1.0, 0.0);

    float pca(0.0);
    if (viewFrom_.y() > 0.0)
    {
        table_.draw(modelview_, projection_, lightPos_, logoPos_, currentTime_, pca);
    }

    glEnable(GL_CULL_FACE); 
    glDisable(GL_DEPTH_TEST); 

    if (logoPos_.y() < 0.0)
    {
        // Set the color assuming we're still under the table.
        uvec3 flatColor(128 / 2,  102 / 2,  179 / 2);
        if (logoPos_.y() > -0.33)
        {
            // We're emerging from the table
            float c(1.0 - logoPos_.y() / -0.33);
            pca /= 4.0;
            flatColor.x(static_cast<unsigned int>(128.0 * (1.0 - c) * 0.5 + 255.0 * pca * c));
            flatColor.y(static_cast<unsigned int>(102.0 * (1.0 - c) * 0.5 + 255.0 * pca * c));
            flatColor.z(static_cast<unsigned int>(179.0 * (1.0 - c) * 0.5 + 200.0 * pca * c));
        }

        modelview_.push();
        modelview_.scale(0.04, 0.0, 0.04);
        modelview_.rotate(-90.0, 1.0, 0.0, 0.0);
        modelview_.rotate(0.1 * static_cast<int>(10.0 * logoRot_.z()), 0.0, 0.0, 1.0);
        modelview_.rotate(0.1 * static_cast<int>(10.0 * logoRot_.y()), 0.0, 1.0, 0.0);
        modelview_.rotate(0.1 * static_cast<int>(10.0 * logoRot_.x()), 1.0, 0.0, 0.0);
        modelview_.rotate(0.1 * 353, 1.0, 0.0, 0.0);
        modelview_.rotate(0.1 * 450, 0.0, 1.0, 0.0);

        logo_.draw(modelview_, projection_, lightPositions_[0], SGILogo::LOGO_FLAT, flatColor);

        modelview_.pop();
    }
    
    if (logoPos_.y() > 0.0)
    {
        modelview_.push();
        modelview_.translate(lightPos_.x(), lightPos_.y(), lightPos_.z());
        mat4 tv;
        tv[3][1] = -1.0;
        tv[3][3] = 0.0;
        tv[0][0] = tv[1][1] = tv[2][2] = lightPos_.y();
        modelview_ *= tv;
        modelview_.translate(-lightPos_.x() + logoPos_.x(),
                             -lightPos_.y() + logoPos_.y(),
                             -lightPos_.z() + logoPos_.z());
        modelview_.scale(0.04, 0.04, 0.04);
        modelview_.rotate(-90.0, 1.0, 0.0, 0.0);
        modelview_.rotate(0.1 * static_cast<int>(10.0 * logoRot_.z()), 0.0, 0.0, 1.0);
        modelview_.rotate(0.1 * static_cast<int>(10.0 * logoRot_.y()), 0.0, 1.0, 0.0);
        modelview_.rotate(0.1 * static_cast<int>(10.0 * logoRot_.x()), 1.0, 0.0, 0.0);
        modelview_.rotate(35.3, 1.0, 0.0, 0.0);
        modelview_.rotate(45.0, 0.0, 1.0, 0.0);

        logo_.draw(modelview_, projection_, lightPositions_[0], SGILogo::LOGO_SHADOW);

        modelview_.pop();
    }
    //
    // DONE SHADOW 
    //

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    modelview_.loadIdentity();
    modelview_.lookAt(viewFrom_.x(), viewFrom_.y(), viewFrom_.z(),
                      viewTo_.x(), viewTo_.y(), viewTo_.z(), 
                      0.0, 1.0, 0.0);
    modelview_.push();
    modelview_.translate(lightPos_.x(), lightPos_.y(), lightPos_.z());
    modelview_.scale(0.1, 0.1, 0.1);
    float x(lightPos_.x() - logoPos_.x());
    float y(lightPos_.y() - logoPos_.y());
    float z(lightPos_.z() - logoPos_.z());
    double a3(0.0);
    if (x != 0.0)
    {
        a3 = -atan2(z, x) * 10.0 * 180.0 / M_PI;
    }
    double a4(-atan2(sqrt(x * x + z * z), y) * 10.0 * 180.0 / M_PI);
    modelview_.rotate(0.1 * static_cast<int>(a3), 0.0, 1.0, 0.0);
    modelview_.rotate(0.1 * static_cast<int>(a4), 0.0, 0.0, 1.0);
    modelview_.rotate(-90.0, 1.0, 0.0, 0.0);

    lamp_.draw(modelview_, projection_, lightPositions_);

    modelview_.pop();
    
    lightPositions_[0] = modelview_.getCurrent() * lp4;
    
    if (logoPos_.y() > -0.33)
    {
        modelview_.push();
        modelview_.translate(logoPos_.x(), logoPos_.y(), logoPos_.z());
        modelview_.scale(0.04, 0.04, 0.04);
        modelview_.rotate(-90.0, 1.0, 0.0, 0.0);
        modelview_.rotate(0.1 * static_cast<int>(10.0 * logoRot_.z()), 0.0, 0.0, 1.0);
        modelview_.rotate(0.1 * static_cast<int>(10.0 * logoRot_.y()), 0.0, 1.0, 0.0);
        modelview_.rotate(0.1 * static_cast<int>(10.0 * logoRot_.x()), 1.0, 0.0, 0.0);
        modelview_.rotate(35.3, 1.0, 0.0, 0.0);
        modelview_.rotate(45.0, 0.0, 1.0, 0.0);

        logo_.draw(modelview_, projection_, lightPositions_[0], SGILogo::LOGO_NORMAL);

        modelview_.pop();
    }
    
    if (viewFrom_.y() < 0.0)
    {
        table_.drawUnder(modelview_, projection_);
    }
}

void
SceneIdeas::draw()
{
    priv_->draw();
}

Scene::ValidationResult
SceneIdeas::validate()
{
    return Scene::ValidationSuccess;
}

void
SceneIdeas::teardown()
{
    Scene::teardown();
}
